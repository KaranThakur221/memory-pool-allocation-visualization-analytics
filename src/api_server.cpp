#include "api_server.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "benchmark.h"
#include "experiment_metrics.h"
#include "experiment_store.h"
#include "memory_pool.h"

namespace {

struct WorkloadEntry {
    std::string operation;
    size_t size = 0;
    uint64_t allocation_id = 0;
};

std::string json_escape(const std::string& input) {
    std::string escaped;
    escaped.reserve(input.size());
    for (char ch : input) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string trim(const std::string& input) {
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = input.find_last_not_of(" \t\r\n");
    return input.substr(start, end - start + 1);
}

std::string to_lower(const std::string& value) {
    std::string out = value;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::string read_request_body(const std::string& request) {
    const auto header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return "";
    }
    const std::string headers = request.substr(0, header_end);
    const std::string body = request.substr(header_end + 4);

    const auto content_length_pos = headers.find("Content-Length:");
    if (content_length_pos == std::string::npos) {
        return body;
    }

    auto value_start = headers.find(':', content_length_pos);
    if (value_start == std::string::npos) {
        return body;
    }
    value_start += 1;
    while (value_start < headers.size() && (headers[value_start] == ' ' || headers[value_start] == '\t')) {
        ++value_start;
    }
    const auto value_end = headers.find("\r\n", value_start);
    const std::string length_text = headers.substr(value_start, value_end - value_start);
    const size_t expected = static_cast<size_t>(std::stoull(length_text));
    return body.size() >= expected ? body.substr(0, expected) : body;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> parts;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        parts.push_back(trim(item));
    }
    return parts;
}

std::string extract_json_value(const std::string& json, const std::string& key) {
    const std::string pattern = std::string("\"") + key + std::string("\"");
    const auto pos = json.find(pattern);
    if (pos == std::string::npos) {
        return "";
    }
    const auto colon = json.find(':', pos + pattern.size());
    if (colon == std::string::npos) {
        return "";
    }
    const auto value_start = json.find_first_not_of(" \t\r\n", colon + 1);
    if (value_start == std::string::npos) {
        return "";
    }
    if (json[value_start] == '"') {
        const auto value_end = json.find('"', value_start + 1);
        return value_end == std::string::npos ? "" : json.substr(value_start + 1, value_end - value_start - 1);
    }
    const auto value_end = json.find_first_of(",}\r\n", value_start);
    return value_end == std::string::npos ? json.substr(value_start) : json.substr(value_start, value_end - value_start);
}

bool is_unsigned_integer(const std::string& value) {
    return !value.empty()
        && std::all_of(value.begin(), value.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        });
}

bool is_valid_uint64(const std::string& value) {
    if (!is_unsigned_integer(value)) {
        return false;
    }
    try {
        size_t consumed = 0;
        std::stoull(value, &consumed);
        return consumed == value.size();
    } catch (...) {
        return false;
    }
}

size_t extract_json_size(const std::string& json, const std::string& key) {
    const std::string value = trim(extract_json_value(json, key));
    if (!is_unsigned_integer(value)) {
        return 0;
    }
    try {
        const unsigned long long parsed = std::stoull(value);
        return parsed > std::numeric_limits<size_t>::max() ? 0 : static_cast<size_t>(parsed);
    } catch (...) {
        return 0;
    }
}

uint64_t extract_json_id(const std::string& json, const std::string& key) {
    const std::string value = trim(extract_json_value(json, key));
    if (!is_unsigned_integer(value)) {
        return 0;
    }
    try {
        return static_cast<uint64_t>(std::stoull(value));
    } catch (...) {
        return 0;
    }
}

std::string make_json_response(bool success, const std::string& message, const std::string& payload) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"success\": " << (success ? "true" : "false") << ",\n";
    out << "  \"message\": \"" << json_escape(message) << "\",\n";
    if (!payload.empty()) {
        out << "  \"payload\": " << payload << "\n";
    }
    out << "}\n";
    return out.str();
}

std::vector<WorkloadEntry> parse_workload_json(const std::string& json) {
    std::vector<WorkloadEntry> workload;
    const std::string op_key = "\"operation\"";
    size_t pos = 0;
    while ((pos = json.find(op_key, pos)) != std::string::npos) {
        const auto object_start = json.rfind('{', pos);
        const auto object_end = json.find('}', pos);
        if (object_start == std::string::npos || object_end == std::string::npos) {
            break;
        }
        const std::string object = json.substr(object_start, object_end - object_start + 1);
        WorkloadEntry entry;
        entry.operation = trim(extract_json_value(object, "operation"));
        entry.size = extract_json_size(object, "size");
        entry.allocation_id = extract_json_id(object, "allocation_id");
        workload.push_back(entry);
        pos = object_end + 1;
    }
    return workload;
}

std::vector<WorkloadEntry> parse_workload_csv(const std::string& csv_text) {
    std::vector<WorkloadEntry> workload;
    std::stringstream ss(csv_text);
    std::string line;
    bool header_seen = false;
    while (std::getline(ss, line)) {
        if (line.empty()) {
            continue;
        }
        const auto parts = split_csv_line(line);
        if (parts.size() < 2) {
            continue;
        }
        if (!header_seen && parts[0] == "operation") {
            header_seen = true;
            continue;
        }
        header_seen = true;
        WorkloadEntry entry;
        entry.operation = trim(parts[0]);
        if (parts.size() > 1 && !parts[1].empty()) {
            try {
                entry.size = static_cast<size_t>(std::stoull(parts[1]));
            } catch (...) {
                entry.size = 0;
            }
        }
        if (parts.size() > 2 && !parts[2].empty()) {
            try {
                entry.allocation_id = static_cast<uint64_t>(std::stoull(parts[2]));
            } catch (...) {
                entry.allocation_id = 0;
            }
        }
        if (!entry.operation.empty()) {
            workload.push_back(entry);
        }
    }
    return workload;
}

std::vector<ExperimentRecord> g_experiments;
std::mutex g_experiments_mutex;

struct WorkloadSession {
    enum class Status { Idle, Running, Paused, Completed, Failed };

    Status status = Status::Idle;
    std::vector<WorkloadEntry> workload;
    size_t next_operation = 0;
    std::string strategy = "first_fit";
    std::string allocation_mode = "variable_size";
    size_t fixed_block_size = 0;
    size_t capacity = 0;
    std::unique_ptr<MemoryPool> pool;
    std::unordered_map<uint64_t, void*> active;
    std::vector<OperationRecord> results;
    std::vector<std::string> errors;
    std::string experiment_id;
};

WorkloadSession g_workload_session;
std::mutex g_workload_session_mutex;

const char* workload_session_status(WorkloadSession::Status status) {
    switch (status) {
        case WorkloadSession::Status::Idle: return "idle";
        case WorkloadSession::Status::Running: return "running";
        case WorkloadSession::Status::Paused: return "paused";
        case WorkloadSession::Status::Completed: return "completed";
        case WorkloadSession::Status::Failed: return "failed";
    }
    return "failed";
}

std::string experiment_store_path() {
    const char* configured_path = std::getenv("MEMORY_POOL_EXPERIMENTS_PATH");
    return configured_path != nullptr && configured_path[0] != '\0'
        ? configured_path
        : "data/experiments.bin";
}

std::vector<WorkloadEntry> parse_workload(const std::string& input) {
    const std::string trimmed = trim(input);
    if (trimmed.empty()) {
        return {};
    }
    if (trimmed[0] == '[' || trimmed[0] == '{') {
        return parse_workload_json(trimmed);
    }
    return parse_workload_csv(trimmed);
}

MemoryPool::Strategy strategy_from_name(const std::string& input) {
    const std::string normalized = to_lower(input);
    if (normalized == "best_fit") {
        return MemoryPool::Strategy::BestFit;
    }
    if (normalized == "fixed_size") {
        return MemoryPool::Strategy::FixedSize;
    }
    if (normalized == "variable_size") {
        return MemoryPool::Strategy::VariableSize;
    }
    return MemoryPool::Strategy::FirstFit;
}

bool is_supported_strategy(const std::string& input) {
    const std::string normalized = to_lower(input);
    return normalized == "first_fit" || normalized == "best_fit"
        || normalized == "fixed_size" || normalized == "variable_size";
}

std::string normalize_algorithm_name(const std::string& input) {
    return to_lower(input) == "best_fit" ? "best_fit" : "first_fit";
}

std::string normalize_allocation_mode(const std::string& algorithm, const std::string& requested_mode) {
    return requested_mode.empty()
        ? (algorithm == "fixed_size" ? "fixed_size" : "variable_size")
        : to_lower(requested_mode);
}

MemoryPool::AllocationMode allocation_mode_from_name(const std::string& input) {
    return to_lower(input) == "fixed_size"
        ? MemoryPool::AllocationMode::FixedSize
        : MemoryPool::AllocationMode::VariableSize;
}

std::string strategy_to_string(MemoryPool::Strategy strategy) {
    switch (strategy) {
        case MemoryPool::Strategy::BestFit: return "best_fit";
        case MemoryPool::Strategy::FixedSize: return "fixed_size";
        case MemoryPool::Strategy::VariableSize: return "variable_size";
        case MemoryPool::Strategy::FirstFit:
        default: return "first_fit";
    }
}

std::string operation_record_to_json(const OperationRecord& record) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"operation\": \"" << json_escape(record.operation) << "\",\n";
    out << "  \"allocation_id\": " << record.allocation_id << ",\n";
    out << "  \"requested_size\": " << record.requested_size << ",\n";
    out << "  \"actual_size\": " << record.actual_size << ",\n";
    out << "  \"offset\": " << record.offset << ",\n";
    out << "  \"success\": " << (record.success ? "true" : "false") << ",\n";
    out << "  \"latency_ms\": " << record.latency_ms << ",\n";
    out << "  \"error_message\": \"" << json_escape(record.error_message) << "\"\n";
    out << "}\n";
    return out.str();
}

std::string workload_session_json(const WorkloadSession& session) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"status\": \"" << workload_session_status(session.status) << "\",\n";
    out << "  \"experiment_id\": \"" << json_escape(session.experiment_id) << "\",\n";
    out << "  \"current_operation\": " << session.next_operation << ",\n";
    out << "  \"total_operations\": " << session.workload.size() << ",\n";
    out << "  \"progress\": " << (session.workload.empty() ? 0.0 : static_cast<double>(session.next_operation) / session.workload.size()) * 100.0 << ",\n";
    out << "  \"configuration\": {\"strategy\": \"" << json_escape(session.strategy)
        << "\", \"allocation_mode\": \"" << json_escape(session.allocation_mode)
        << "\", \"fixed_block_size\": " << session.fixed_block_size << "},\n";
    out << "  \"results\": [";
    for (size_t i = 0; i < session.results.size(); ++i) {
        if (i > 0) out << ',';
        out << operation_record_to_json(session.results[i]);
    }
    out << "],\n";
    out << "  \"state\": " << (session.pool ? session.pool->to_json() : "{}") << "\n";
    out << "}\n";
    return out.str();
}

OperationRecord execute_session_operation(WorkloadSession& session) {
    const auto& item = session.workload[session.next_operation];
    const auto start = std::chrono::steady_clock::now();
    OperationRecord record;
    record.operation = to_lower(item.operation);
    record.allocation_id = item.allocation_id;
    record.requested_size = item.size;

    if (record.operation == "allocate") {
        if (record.allocation_id == 0) {
            record.allocation_id = static_cast<uint64_t>(session.results.size() + 1);
        }
        void* pointer = session.active.count(record.allocation_id) == 0
            ? session.pool->allocate(record.requested_size, record.allocation_id)
            : nullptr;
        record.success = pointer != nullptr;
        if (pointer != nullptr) {
            session.active[record.allocation_id] = pointer;
            const auto blocks = session.pool->blocks();
            const auto block = std::find_if(blocks.begin(), blocks.end(), [&record](const AllocationBlock& candidate) {
                return candidate.allocated && candidate.allocation_id == record.allocation_id;
            });
            if (block != blocks.end()) {
                record.offset = block->offset;
                record.actual_size = block->size;
            }
        } else {
            record.error_message = session.active.count(record.allocation_id) > 0
                ? "Allocation id already active"
                : "Allocation failed";
        }
    } else if (record.operation == "free") {
        const auto allocation = session.active.find(record.allocation_id);
        if (allocation != session.active.end()) {
            const auto blocks = session.pool->blocks();
            const auto block = std::find_if(blocks.begin(), blocks.end(), [&record](const AllocationBlock& candidate) {
                return candidate.allocated && candidate.allocation_id == record.allocation_id;
            });
            if (block != blocks.end()) record.offset = block->offset;
            record.success = session.pool->deallocate(allocation->second);
            if (record.success) {
                session.active.erase(allocation);
            } else {
                record.error_message = "Deallocation failed";
            }
        } else {
            record.error_message = "Invalid allocation id";
        }
    } else {
        record.error_message = "Unsupported operation";
    }

    const auto end = std::chrono::steady_clock::now();
    record.latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
    return record;
}

bool finalize_workload_session(
    WorkloadSession& session,
    const ExperimentStore& store,
    ExperimentRecord& record,
    std::string& error) {
    record.experiment_id = session.experiment_id;
    record.name = "Stepwise workload";
    record.timestamp = iso_timestamp_now();
    record.pool_capacity = session.capacity;
    record.strategy = session.strategy;
    record.allocation_mode = session.allocation_mode;
    record.fixed_block_size = session.fixed_block_size;
    record.workload_configuration = std::to_string(session.workload.size()) + " workload operations";
    record.operation_sequence = session.results;
    record.final_memory_state_json = session.pool->to_json();
    record.random_seed = 0;
    std::ostringstream signature;
    double allocation_time = 0.0;
    double deallocation_time = 0.0;
    size_t allocation_requests = 0;
    size_t successful_allocations = 0;
    size_t deallocation_requests = 0;
    size_t successful_deallocations = 0;
    for (const auto& operation : session.results) {
        signature << operation.operation << ':' << operation.requested_size << ':' << operation.allocation_id << ';';
        if (operation.operation == "allocate") {
            ++allocation_requests;
            allocation_time += operation.latency_ms;
            if (operation.success) ++successful_allocations;
        } else {
            ++deallocation_requests;
            deallocation_time += operation.latency_ms;
            if (operation.success) ++successful_deallocations;
        }
    }
    record.workload_signature = signature.str();
    record.workload_metrics.total_operations = session.results.size();
    record.workload_metrics.allocation_operations = allocation_requests;
    record.workload_metrics.deallocation_operations = deallocation_requests;
    record.workload_metrics.successful_operations = successful_allocations + successful_deallocations;
    record.workload_metrics.failed_operations = record.workload_metrics.total_operations
        - record.workload_metrics.successful_operations;
    record.memory_metrics.total_capacity = session.pool->stats().total_capacity;
    record.memory_metrics.total_allocated_memory = session.pool->stats().allocated_memory;
    record.memory_metrics.total_free_memory = session.pool->stats().free_memory;
    record.memory_metrics.peak_memory_usage = session.pool->stats().peak_memory_usage;
    record.allocation_metrics.total_requests = allocation_requests;
    record.allocation_metrics.successful_requests = successful_allocations;
    record.allocation_metrics.failed_requests = allocation_requests - successful_allocations;
    record.allocation_metrics.total_time_ms = allocation_time;
    record.allocation_metrics.average_latency_ms = allocation_requests == 0 ? 0.0 : allocation_time / allocation_requests;
    record.deallocation_metrics.total_requests = deallocation_requests;
    record.deallocation_metrics.successful_requests = successful_deallocations;
    record.deallocation_metrics.failed_requests = deallocation_requests - successful_deallocations;
    record.deallocation_metrics.total_time_ms = deallocation_time;
    record.deallocation_metrics.average_latency_ms = deallocation_requests == 0 ? 0.0 : deallocation_time / deallocation_requests;
    record.benchmark_results = compare_experiments({record});
    return store.append(record, error);
}

std::string benchmark_result_to_json(const AlgorithmBenchmarkResult& result) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"experiment_id\": \"" << json_escape(result.experiment_id) << "\",\n";
    out << "  \"name\": \"" << json_escape(result.name) << "\",\n";
    out << "  \"strategy\": \"" << json_escape(result.strategy) << "\",\n";
    out << "  \"allocation_mode\": \"" << json_escape(result.allocation_mode) << "\",\n";
    out << "  \"allocation_requests\": " << result.allocation_requests << ",\n";
    out << "  \"successful_allocations\": " << result.successful_allocations << ",\n";
    out << "  \"failed_allocations\": " << result.failed_allocations << ",\n";
    out << "  \"deallocation_requests\": " << result.deallocation_requests << ",\n";
    out << "  \"successful_deallocations\": " << result.successful_deallocations << ",\n";
    out << "  \"failed_deallocations\": " << result.failed_deallocations << ",\n";
    out << "  \"total_allocation_time_ms\": " << result.total_allocation_time_ms << ",\n";
    out << "  \"average_deallocation_latency_ms\": " << result.average_deallocation_latency_ms << ",\n";
    out << "  \"maximum_allocation_latency_ms\": " << result.maximum_allocation_latency_ms << ",\n";
    out << "  \"total_deallocation_time_ms\": " << result.total_deallocation_time_ms << ",\n";
    out << "  \"maximum_deallocation_latency_ms\": " << result.maximum_deallocation_latency_ms << ",\n";
    out << "  \"execution_time_ms\": " << result.execution_time_ms << ",\n";
    out << "  \"average_allocation_latency_ms\": " << result.average_allocation_latency_ms << ",\n";
    out << "  \"p95_allocation_latency_ms\": " << result.p95_allocation_latency_ms << ",\n";
    out << "  \"p99_allocation_latency_ms\": " << result.p99_allocation_latency_ms << ",\n";
    out << "  \"allocation_success_rate\": " << result.allocation_success_rate << ",\n";
    out << "  \"allocation_failure_rate\": " << result.allocation_failure_rate << ",\n";
    out << "  \"peak_memory_utilization\": " << result.peak_memory_utilization << ",\n";
    out << "  \"peak_memory_usage\": " << result.peak_memory_usage << ",\n";
    out << "  \"final_memory_utilization\": " << result.final_memory_utilization << ",\n";
    out << "  \"internal_fragmentation\": " << result.internal_fragmentation << ",\n";
    out << "  \"external_fragmentation\": " << result.external_fragmentation << ",\n";
    out << "  \"largest_free_block\": " << result.largest_free_block << ",\n";
    out << "  \"free_block_count\": " << result.free_block_count << ",\n";
    out << "  \"block_splits\": " << result.block_splits << ",\n";
    out << "  \"coalescing_operations\": " << result.coalescing_operations << "\n";
    out << "}\n";
    return out.str();
}

AllocationLatencyStats summarize_latencies(const std::vector<double>& values, size_t successful_count, size_t failed_count, size_t total_requests) {
    AllocationLatencyStats stats;
    stats.total_requests = total_requests;
    stats.successful_requests = successful_count;
    stats.failed_requests = failed_count;
    if (values.empty()) {
        return stats;
    }

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    double sum = 0.0;
    for (double value : sorted) {
        sum += value;
    }
    stats.total_time_ms = sum;
    stats.average_latency_ms = sum / static_cast<double>(sorted.size());
    stats.min_latency_ms = sorted.front();
    stats.max_latency_ms = sorted.back();
    stats.median_latency_ms = sorted.size() % 2 == 0
        ? (sorted[sorted.size() / 2 - 1] + sorted[sorted.size() / 2]) / 2.0
        : sorted[sorted.size() / 2];
    const auto p95 = percentile_values(sorted, 95.0);
    const auto p99 = percentile_values(sorted, 99.0);
    stats.p95_latency_ms = p95.empty() ? 0.0 : p95.back();
    stats.p99_latency_ms = p99.empty() ? 0.0 : p99.back();
    stats.success_rate = total_requests == 0 ? 0.0 : static_cast<double>(successful_count) / static_cast<double>(total_requests);
    stats.failure_rate = total_requests == 0 ? 0.0 : static_cast<double>(failed_count) / static_cast<double>(total_requests);
    return stats;
}

double safe_divide(double numerator, double denominator) {
    return denominator == 0.0 ? 0.0 : numerator / denominator;
}

ExperimentRecord run_workload_experiment(
    const std::string& strategy_name,
    const std::string& allocation_mode,
    size_t capacity,
    const std::vector<WorkloadEntry>& workload,
    size_t fixed_block_size,
    uint64_t random_seed,
    const std::string& experiment_name) {
    ExperimentRecord record;
    record.experiment_id = generate_experiment_id();
    record.name = experiment_name.empty() ? "Allocator Experiment " + record.experiment_id : experiment_name;
    record.timestamp = iso_timestamp_now();
    record.pool_capacity = capacity;
    record.strategy = normalize_algorithm_name(strategy_name);
    record.allocation_mode = allocation_mode;
    record.fixed_block_size = allocation_mode == "fixed_size" ? fixed_block_size : 0;
    record.random_seed = random_seed;
    record.workload_configuration = std::to_string(workload.size()) + " workload operations";
    std::ostringstream workload_signature;
    for (const auto& item : workload) {
        workload_signature << to_lower(item.operation) << ':' << item.size << ':' << item.allocation_id << ';';
    }
    record.workload_signature = workload_signature.str();

    MemoryPool pool(
        capacity,
        record.fixed_block_size,
        strategy_from_name(record.strategy),
        allocation_mode_from_name(allocation_mode));
    std::unordered_map<uint64_t, void*> active;
    std::unordered_map<uint64_t, size_t> active_requested_sizes;
    std::vector<double> allocation_times_ms;
    std::vector<double> deallocation_times_ms;
    std::vector<double> utilization_samples;
    std::vector<size_t> requested_sizes_for_distribution;
    std::vector<std::string> errors;
    size_t total_requested_memory = 0;
    size_t peak_active = 0;
    size_t successful_operation_count = 0;
    size_t failed_operation_count = 0;
    size_t alloc_op_count = 0;
    size_t dealloc_op_count = 0;
    size_t successful_alloc_count = 0;
    size_t failed_alloc_count = 0;
    size_t successful_dealloc_count = 0;
    size_t failed_dealloc_count = 0;
    auto overall_start = std::chrono::steady_clock::now();

    for (const auto& item : workload) {
        const auto start = std::chrono::steady_clock::now();
        if (to_lower(item.operation) == "allocate") {
            ++alloc_op_count;
            const size_t user_requested_size = item.size;
            requested_sizes_for_distribution.push_back(user_requested_size);
            uint64_t allocation_id = item.allocation_id == 0
                ? static_cast<uint64_t>(alloc_op_count)
                : item.allocation_id;
            while (item.allocation_id == 0 && active.find(allocation_id) != active.end()) {
                ++allocation_id;
            }
            const bool duplicate_active_id = active.find(allocation_id) != active.end();
            void* ptr = duplicate_active_id ? nullptr : pool.allocate(user_requested_size, allocation_id);
            const auto end = std::chrono::steady_clock::now();
            const double latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
            allocation_times_ms.push_back(latency_ms);
            total_requested_memory += user_requested_size;
            if (ptr != nullptr) {
                active[allocation_id] = ptr;
                active_requested_sizes[allocation_id] = user_requested_size;
                ++successful_alloc_count;
                ++successful_operation_count;
            } else {
                ++failed_alloc_count;
                ++failed_operation_count;
                errors.push_back(duplicate_active_id
                    ? "Allocation id already active: " + std::to_string(allocation_id)
                    : "Allocation failed: insufficient contiguous memory for request size " + std::to_string(user_requested_size));
            }
            const MemoryStats snapshot = pool.stats();
            peak_active = std::max(peak_active, active.size());
            utilization_samples.push_back(safe_divide(static_cast<double>(snapshot.allocated_memory), static_cast<double>(snapshot.total_capacity)) * 100.0);
            OperationRecord op;
            op.operation = "allocate";
            op.allocation_id = allocation_id;
            op.requested_size = user_requested_size;
            op.success = ptr != nullptr;
            op.latency_ms = latency_ms;
            if (ptr != nullptr) {
                const auto current_blocks = pool.blocks();
                const auto allocated_block = std::find_if(current_blocks.begin(), current_blocks.end(), [allocation_id](const AllocationBlock& block) {
                    return block.allocated && block.allocation_id == allocation_id;
                });
                if (allocated_block != current_blocks.end()) {
                    op.offset = allocated_block->offset;
                    op.actual_size = allocated_block->size;
                }
            }
            op.error_message = ptr == nullptr
                ? (duplicate_active_id ? "Allocation id already active" : "Allocation failed")
                : "";
            record.operation_sequence.push_back(op);
        } else if (to_lower(item.operation) == "free") {
            ++dealloc_op_count;
            auto it = active.find(item.allocation_id);
            bool deallocated = false;
            size_t allocation_offset = 0;
            double latency_ms = 0.0;
            if (it != active.end()) {
                const auto current_blocks = pool.blocks();
                const auto allocated_block = std::find_if(current_blocks.begin(), current_blocks.end(), [&item](const AllocationBlock& block) {
                    return block.allocated && block.allocation_id == item.allocation_id;
                });
                if (allocated_block != current_blocks.end()) allocation_offset = allocated_block->offset;
                const auto deallocation_start = std::chrono::steady_clock::now();
                deallocated = pool.deallocate(it->second);
                const auto deallocation_end = std::chrono::steady_clock::now();
                latency_ms = std::chrono::duration<double, std::milli>(deallocation_end - deallocation_start).count();
                if (deallocated) {
                    active.erase(it);
                    active_requested_sizes.erase(item.allocation_id);
                    ++successful_dealloc_count;
                    ++successful_operation_count;
                } else {
                    ++failed_dealloc_count;
                    ++failed_operation_count;
                    errors.push_back("Deallocation failed for allocation id " + std::to_string(item.allocation_id));
                }
            } else {
                ++failed_dealloc_count;
                ++failed_operation_count;
                errors.push_back("Invalid free for allocation id " + std::to_string(item.allocation_id));
                const auto end = std::chrono::steady_clock::now();
                latency_ms = std::chrono::duration<double, std::milli>(end - start).count();
            }
            deallocation_times_ms.push_back(latency_ms);
            const MemoryStats snapshot = pool.stats();
            utilization_samples.push_back(safe_divide(static_cast<double>(snapshot.allocated_memory), static_cast<double>(snapshot.total_capacity)) * 100.0);
            OperationRecord op;
            op.operation = "free";
            op.allocation_id = item.allocation_id;
            op.requested_size = 0;
            op.actual_size = 0;
            op.offset = allocation_offset;
            op.success = deallocated;
            op.latency_ms = latency_ms;
            op.error_message = deallocated ? "" : "Deallocation failed";
            record.operation_sequence.push_back(op);
        }
    }

    const auto overall_end = std::chrono::steady_clock::now();
    record.workload_metrics.total_operations = workload.size();
    record.workload_metrics.allocation_operations = alloc_op_count;
    record.workload_metrics.deallocation_operations = dealloc_op_count;
    record.workload_metrics.successful_operations = successful_operation_count;
    record.workload_metrics.failed_operations = failed_operation_count;
    record.workload_metrics.workload_execution_duration_ms = std::chrono::duration<double, std::milli>(overall_end - overall_start).count();
    record.workload_metrics.allocation_size_distribution = build_size_distribution(requested_sizes_for_distribution);
    record.workload_metrics.peak_active_allocations = peak_active;
    record.workload_metrics.total_memory_requested_throughout_workload = total_requested_memory;

    const MemoryStats final_stats = pool.stats();
    size_t final_requested_memory = 0;
    for (const auto& allocation : active_requested_sizes) {
        final_requested_memory += allocation.second;
    }
    record.memory_metrics.total_capacity = final_stats.total_capacity;
    record.memory_metrics.total_requested_memory = final_requested_memory;
    record.memory_metrics.total_allocated_memory = final_stats.allocated_memory;
    record.memory_metrics.total_free_memory = final_stats.free_memory;
    record.memory_metrics.peak_memory_usage = final_stats.peak_memory_usage;
    record.memory_metrics.average_memory_utilization = utilization_samples.empty() ? 0.0 : std::accumulate(utilization_samples.begin(), utilization_samples.end(), 0.0) / static_cast<double>(utilization_samples.size());
    record.memory_metrics.peak_memory_utilization = final_stats.peak_memory_utilization * 100.0;

    record.allocation_metrics = summarize_latencies(allocation_times_ms, successful_alloc_count, failed_alloc_count, alloc_op_count);
    record.deallocation_metrics = summarize_latencies(deallocation_times_ms, successful_dealloc_count, failed_dealloc_count, dealloc_op_count);

    record.fragmentation.internal_fragmentation_bytes = final_stats.allocated_memory >= final_requested_memory
        ? static_cast<double>(final_stats.allocated_memory - final_requested_memory)
        : 0.0;
    record.fragmentation.internal_fragmentation_percent = final_requested_memory == 0
        ? 0.0
        : (100.0 * record.fragmentation.internal_fragmentation_bytes / static_cast<double>(final_requested_memory));
    record.fragmentation.external_fragmentation_ratio = final_stats.external_fragmentation * 100.0;
    record.fragmentation.total_free_blocks = final_stats.free_block_count;
    record.fragmentation.largest_free_block = final_stats.largest_free_block;
    record.fragmentation.average_free_block_size = final_stats.average_free_block_size;
    record.fragmentation.contiguous_memory_failures = final_stats.contiguous_memory_failures;
    record.fragmentation.successful_coalescing_operations = final_stats.successful_coalescing_operations;
    record.fragmentation.block_splitting_operations = final_stats.block_splitting_operations;

    record.final_memory_state_json = pool.to_json();
    record.errors = errors;
    return record;
}

bool generate_random_workload(
    size_t capacity,
    size_t operation_count,
    size_t minimum_size,
    size_t maximum_size,
    size_t allocation_probability,
    size_t deallocation_probability,
    uint64_t seed,
    const std::string& strategy_name,
    const std::string& allocation_mode,
    size_t fixed_block_size,
    std::vector<WorkloadEntry>& workload,
    std::string& error) {
    constexpr size_t kMaximumRandomOperations = 10000;
    if (capacity == 0 || operation_count == 0 || operation_count > kMaximumRandomOperations
        || minimum_size == 0 || maximum_size < minimum_size || maximum_size > capacity
        || allocation_probability > 100 || deallocation_probability > 100
        || allocation_probability + deallocation_probability != 100) {
        error = "Invalid random workload configuration; operation count must be 1-10000, sizes must be in range, and probabilities must total 100%.";
        return false;
    }
    if (strategy_name != "first_fit" && strategy_name != "best_fit") {
        error = "Allocation algorithm must be first_fit or best_fit.";
        return false;
    }
    if (allocation_mode != "fixed_size" && allocation_mode != "variable_size") {
        error = "Allocation mode must be fixed_size or variable_size.";
        return false;
    }
    if (allocation_mode == "fixed_size"
        && (fixed_block_size == 0 || fixed_block_size > capacity || maximum_size > fixed_block_size)) {
        error = "Fixed-size random requests must fit within the configured fixed block.";
        return false;
    }

    std::mt19937_64 generator(seed);
    std::uniform_int_distribution<size_t> size_distribution(minimum_size, maximum_size);
    std::uniform_int_distribution<size_t> probability_distribution(
        1, allocation_probability + deallocation_probability);
    std::vector<uint64_t> active;
    uint64_t next_id = 1;
    workload.clear();
    workload.reserve(operation_count);

    for (size_t operation_index = 0; operation_index < operation_count; ++operation_index) {
        const bool allocate = active.empty()
            || probability_distribution(generator) <= allocation_probability;
        if (allocate) {
            const uint64_t allocation_id = next_id++;
            const size_t requested_size = size_distribution(generator);
            workload.push_back({"allocate", requested_size, allocation_id});
            active.push_back(allocation_id);
        } else {
            std::uniform_int_distribution<size_t> active_index(0, active.size() - 1);
            const size_t selected = active_index(generator);
            const uint64_t allocation_id = active[selected];
            workload.push_back({"free", 0, allocation_id});
            active.erase(active.begin() + static_cast<std::ptrdiff_t>(selected));
        }
    }
    return true;
}

bool persist_experiment(
    const ExperimentStore& store,
    const ExperimentRecord& experiment,
    std::string& error) {
    std::lock_guard<std::mutex> lock(g_experiments_mutex);
    if (!store.append(experiment, error)) {
        return false;
    }
    g_experiments.push_back(experiment);
    return true;
}

std::string experiment_record_to_json(const ExperimentRecord& record) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"experiment_id\": \"" << json_escape(record.experiment_id) << "\",\n";
    out << "  \"name\": \"" << json_escape(record.name) << "\",\n";
    out << "  \"timestamp\": \"" << json_escape(record.timestamp) << "\",\n";
    out << "  \"strategy\": \"" << json_escape(record.strategy) << "\",\n";
    out << "  \"allocation_mode\": \"" << json_escape(record.allocation_mode) << "\",\n";
    out << "  \"workload_configuration\": \"" << json_escape(record.workload_configuration) << "\",\n";
    out << "  \"workload_signature\": \"" << json_escape(record.workload_signature) << "\",\n";
    out << "  \"random_seed\": " << record.random_seed << ",\n";
    out << "  \"pool_capacity\": " << record.pool_capacity << ",\n";
    out << "  \"fixed_block_size\": " << record.fixed_block_size << ",\n";
    out << "  \"workload_metrics\": {\n";
    out << "    \"total_operations\": " << record.workload_metrics.total_operations << ",\n";
    out << "    \"allocation_operations\": " << record.workload_metrics.allocation_operations << ",\n";
    out << "    \"deallocation_operations\": " << record.workload_metrics.deallocation_operations << ",\n";
    out << "    \"successful_operations\": " << record.workload_metrics.successful_operations << ",\n";
    out << "    \"failed_operations\": " << record.workload_metrics.failed_operations << ",\n";
    out << "    \"workload_execution_duration_ms\": " << record.workload_metrics.workload_execution_duration_ms << ",\n";
    out << "    \"peak_active_allocations\": " << record.workload_metrics.peak_active_allocations << ",\n";
    out << "    \"total_memory_requested_throughout_workload\": " << record.workload_metrics.total_memory_requested_throughout_workload << ",\n";
    out << "    \"allocation_size_distribution\": [";
    for (size_t i = 0; i < record.workload_metrics.allocation_size_distribution.size(); ++i) {
        const auto& entry = record.workload_metrics.allocation_size_distribution[i];
        out << "{\"size\":" << entry.first << ",\"count\":" << entry.second << '}';
        if (i + 1 < record.workload_metrics.allocation_size_distribution.size()) out << ',';
    }
    out << "]\n";
    out << "  },\n";
    out << "  \"operation_sequence\": [\n";
    for (size_t i = 0; i < record.operation_sequence.size(); ++i) {
        out << operation_record_to_json(record.operation_sequence[i]);
        if (i + 1 < record.operation_sequence.size()) {
            out << ",\n";
        }
    }
    out << "\n  ],\n";
    out << "  \"memory_metrics\": {\n";
    out << "    \"total_capacity\": " << record.memory_metrics.total_capacity << ",\n";
    out << "    \"total_requested_memory\": " << record.memory_metrics.total_requested_memory << ",\n";
    out << "    \"total_allocated_memory\": " << record.memory_metrics.total_allocated_memory << ",\n";
    out << "    \"total_free_memory\": " << record.memory_metrics.total_free_memory << ",\n";
    out << "    \"peak_memory_usage\": " << record.memory_metrics.peak_memory_usage << ",\n";
    out << "    \"average_memory_utilization\": " << record.memory_metrics.average_memory_utilization << ",\n";
    out << "    \"peak_memory_utilization\": " << record.memory_metrics.peak_memory_utilization << "\n";
    out << "  },\n";
    out << "  \"allocation_metrics\": {\n";
    out << "    \"total_requests\": " << record.allocation_metrics.total_requests << ",\n";
    out << "    \"successful_requests\": " << record.allocation_metrics.successful_requests << ",\n";
    out << "    \"failed_requests\": " << record.allocation_metrics.failed_requests << ",\n";
    out << "    \"total_time_ms\": " << record.allocation_metrics.total_time_ms << ",\n";
    out << "    \"average_latency_ms\": " << record.allocation_metrics.average_latency_ms << ",\n";
    out << "    \"median_latency_ms\": " << record.allocation_metrics.median_latency_ms << ",\n";
    out << "    \"min_latency_ms\": " << record.allocation_metrics.min_latency_ms << ",\n";
    out << "    \"max_latency_ms\": " << record.allocation_metrics.max_latency_ms << ",\n";
    out << "    \"p95_latency_ms\": " << record.allocation_metrics.p95_latency_ms << ",\n";
    out << "    \"p99_latency_ms\": " << record.allocation_metrics.p99_latency_ms << ",\n";
    out << "    \"success_rate\": " << record.allocation_metrics.success_rate << ",\n";
    out << "    \"failure_rate\": " << record.allocation_metrics.failure_rate << "\n";
    out << "  },\n";
    out << "  \"deallocation_metrics\": {\n";
    out << "    \"total_requests\": " << record.deallocation_metrics.total_requests << ",\n";
    out << "    \"successful_requests\": " << record.deallocation_metrics.successful_requests << ",\n";
    out << "    \"failed_requests\": " << record.deallocation_metrics.failed_requests << ",\n";
    out << "    \"total_time_ms\": " << record.deallocation_metrics.total_time_ms << ",\n";
    out << "    \"average_latency_ms\": " << record.deallocation_metrics.average_latency_ms << ",\n";
    out << "    \"max_latency_ms\": " << record.deallocation_metrics.max_latency_ms << ",\n";
    out << "    \"success_rate\": " << record.deallocation_metrics.success_rate << ",\n";
    out << "    \"failure_rate\": " << record.deallocation_metrics.failure_rate << "\n";
    out << "  },\n";
    out << "  \"fragmentation\": {\n";
    out << "    \"internal_fragmentation_bytes\": " << record.fragmentation.internal_fragmentation_bytes << ",\n";
    out << "    \"internal_fragmentation_percent\": " << record.fragmentation.internal_fragmentation_percent << ",\n";
    out << "    \"external_fragmentation_ratio\": " << record.fragmentation.external_fragmentation_ratio << ",\n";
    out << "    \"largest_free_block\": " << record.fragmentation.largest_free_block << ",\n";
    out << "    \"average_free_block_size\": " << record.fragmentation.average_free_block_size << ",\n";
    out << "    \"total_free_blocks\": " << record.fragmentation.total_free_blocks << ",\n";
    out << "    \"contiguous_memory_failures\": " << record.fragmentation.contiguous_memory_failures << ",\n";
    out << "    \"successful_coalescing_operations\": " << record.fragmentation.successful_coalescing_operations << ",\n";
    out << "    \"block_splitting_operations\": " << record.fragmentation.block_splitting_operations << "\n";
    out << "  },\n";
    out << "  \"benchmark_results\": [";
    for (size_t i = 0; i < record.benchmark_results.size(); ++i) {
        out << benchmark_result_to_json(record.benchmark_results[i]);
        if (i + 1 < record.benchmark_results.size()) out << ',';
    }
    out << "],\n";
    out << "  \"errors\": [";
    for (size_t i = 0; i < record.errors.size(); ++i) {
        out << '"' << json_escape(record.errors[i]) << '"';
        if (i + 1 < record.errors.size()) out << ',';
    }
    out << "],\n";
    out << "  \"final_memory_state\": " << record.final_memory_state_json << '\n';
    out << "}\n";
    return out.str();
}

std::string experiment_summary_to_json(const ExperimentRecord& record) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"experiment_id\": \"" << json_escape(record.experiment_id) << "\",\n";
    out << "  \"name\": \"" << json_escape(record.name) << "\",\n";
    out << "  \"timestamp\": \"" << json_escape(record.timestamp) << "\",\n";
    out << "  \"strategy\": \"" << json_escape(record.strategy) << "\",\n";
    out << "  \"allocation_mode\": \"" << json_escape(record.allocation_mode) << "\",\n";
    out << "  \"workload_configuration\": \"" << json_escape(record.workload_configuration) << "\",\n";
    out << "  \"workload_signature\": \"" << json_escape(record.workload_signature) << "\",\n";
    out << "  \"random_seed\": " << record.random_seed << ",\n";
    out << "  \"pool_capacity\": " << record.pool_capacity << ",\n";
    out << "  \"fixed_block_size\": " << record.fixed_block_size << ",\n";
    out << "  \"total_operations\": " << record.workload_metrics.total_operations << ",\n";
    out << "  \"successful_operations\": " << record.workload_metrics.successful_operations << ",\n";
    out << "  \"failed_operations\": " << record.workload_metrics.failed_operations << ",\n";
    out << "  \"execution_time_ms\": " << record.workload_metrics.workload_execution_duration_ms << ",\n";
    out << "  \"average_allocation_latency_ms\": " << record.allocation_metrics.average_latency_ms << ",\n";
    out << "  \"average_deallocation_latency_ms\": " << record.deallocation_metrics.average_latency_ms << ",\n";
    out << "  \"successful_allocations\": " << record.allocation_metrics.successful_requests << ",\n";
    out << "  \"failed_allocations\": " << record.allocation_metrics.failed_requests << ",\n";
    out << "  \"allocation_success_rate\": " << record.allocation_metrics.success_rate << ",\n";
    out << "  \"allocation_failure_rate\": " << record.allocation_metrics.failure_rate << ",\n";
    out << "  \"allocated_memory\": " << record.memory_metrics.total_allocated_memory << ",\n";
    out << "  \"free_memory\": " << record.memory_metrics.total_free_memory << ",\n";
    out << "  \"peak_memory_utilization\": " << record.memory_metrics.peak_memory_utilization << ",\n";
    out << "  \"internal_fragmentation_percent\": " << record.fragmentation.internal_fragmentation_percent << ",\n";
    out << "  \"external_fragmentation_ratio\": " << record.fragmentation.external_fragmentation_ratio << ",\n";
    out << "  \"largest_free_block\": " << record.fragmentation.largest_free_block << ",\n";
    out << "  \"free_block_count\": " << record.fragmentation.total_free_blocks << "\n";
    out << "}\n";
    return out.str();
}

std::string parse_query_value(const std::string& query, const std::string& key) {
    const std::string key_pattern = key + "=";
    const auto start = query.find(key_pattern);
    if (start == std::string::npos) {
        return "";
    }
    const auto value_start = start + key_pattern.size();
    const auto value_end = query.find('&', value_start);
    const std::string value = query.substr(value_start, value_end == std::string::npos ? std::string::npos : value_end - value_start);
    return value;
}

std::vector<ExperimentRecord> get_experiments_for_ids(const std::vector<std::string>& ids) {
    std::vector<ExperimentRecord> selected;
    for (const auto& id : ids) {
        for (const auto& record : g_experiments) {
            if (record.experiment_id == id) {
                selected.push_back(record);
                break;
            }
        }
    }
    return selected;
}

}  // namespace

PoolApiServer::PoolApiServer(size_t pool_capacity, unsigned short port)
    : pool_capacity_(pool_capacity), port_(port) {}

PoolApiServer::~PoolApiServer() {
    running_ = false;
}

int PoolApiServer::run() {
    ExperimentStore experiment_store(experiment_store_path());
    std::vector<ExperimentRecord> loaded_experiments;
    std::string store_error;
    if (!experiment_store.load(loaded_experiments, store_error)) {
        std::cerr << "Failed to load experiment history: " << store_error << "\n";
        return 1;
    }
    {
        std::lock_guard<std::mutex> lock(g_experiments_mutex);
        g_experiments = std::move(loaded_experiments);
    }

    auto pool = std::make_unique<MemoryPool>(pool_capacity_, 64, MemoryPool::Strategy::FirstFit);
    std::unordered_map<uint64_t, void*> active_allocations;
    const int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << "\n";
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 16) < 0) {
        std::cerr << "Failed to listen on socket: " << strerror(errno) << "\n";
        close(server_fd);
        return 1;
    }

    running_ = true;
    std::cout << "Memory pool API server listening on http://localhost:" << port_ << "\n";

    while (running_) {
        const int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::cerr << "Failed to accept client: " << strerror(errno) << "\n";
            break;
        }

        std::string request;
        char buffer[4096];
        ssize_t received = 0;
        while ((received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[received] = '\0';
            request.append(buffer, static_cast<size_t>(received));

            const auto header_end = request.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                const auto content_length_pos = request.find("Content-Length:");
                if (content_length_pos != std::string::npos) {
                    const auto value_start = request.find(':', content_length_pos);
                    if (value_start != std::string::npos) {
                        const auto value_end = request.find("\r\n", value_start + 1);
                        const std::string length_text = request.substr(value_start + 1, value_end - value_start - 1);
                        const size_t length = static_cast<size_t>(std::stoull(length_text));
                        const size_t body_length = request.size() - (header_end + 4);
                        if (body_length >= length) {
                            break;
                        }
                    }
                } else {
                    break;
                }
            }
        }

        const auto request_line_end = request.find("\r\n");
        const std::string request_line = request_line_end == std::string::npos ? request : request.substr(0, request_line_end);

        std::string method = "GET";
        std::string path = "/";
        if (!request_line.empty()) {
            const auto first_space = request_line.find(' ');
            const auto second_space = request_line.find(' ', first_space + 1);
            if (first_space != std::string::npos) {
                method = request_line.substr(0, first_space);
                if (second_space != std::string::npos) {
                    path = request_line.substr(first_space + 1, second_space - first_space - 1);
                }
            }
        }

        std::string body = read_request_body(request);
        std::string response_body;
        std::string response_message = "OK";
        bool success = true;

        if (path == "/api/pool/initialize" && method == "POST") {
            const size_t capacity = extract_json_size(body, "capacity");
            const size_t fixed_block_size = extract_json_size(body, "fixed_block_size");
            const std::string strategy_name = extract_json_value(body, "strategy");
            constexpr size_t kMaximumPoolCapacity = 64 * 1024 * 1024;
            if (capacity == 0 || capacity > kMaximumPoolCapacity) {
                success = false;
                response_message = "Pool capacity must be between 1 and 67108864 bytes.";
                response_body = make_json_response(false, response_message, "{}");
            } else if (!is_supported_strategy(strategy_name)) {
                success = false;
                response_message = "Unsupported allocation strategy.";
                response_body = make_json_response(false, response_message, "{}");
            } else if (strategy_name == "fixed_size" && (fixed_block_size == 0 || fixed_block_size > capacity)) {
                success = false;
                response_message = "Fixed block size must be greater than zero and no larger than pool capacity.";
                response_body = make_json_response(false, response_message, "{}");
            } else {
                try {
                    auto replacement = std::make_unique<MemoryPool>(
                        capacity,
                        fixed_block_size == 0 ? 64 : fixed_block_size,
                        strategy_from_name(strategy_name));
                    if (!replacement->is_initialized()) {
                        success = false;
                        response_message = "The allocator could not initialize this pool configuration.";
                        response_body = make_json_response(false, response_message, "{}");
                    } else {
                        pool.swap(replacement);
                        active_allocations.clear();
                        next_allocation_id_ = 1;
                        response_message = "Pool initialized";
                        response_body = make_json_response(true, response_message, pool->to_json());
                    }
                } catch (const std::exception& error) {
                    success = false;
                    response_message = std::string("Pool initialization failed: ") + error.what();
                    response_body = make_json_response(false, response_message, "{}");
                }
            }
        } else if (path == "/api/pool/state" && method == "GET") {
            response_body = make_json_response(true, "Pool state", pool->to_json());
        } else if (path == "/api/statistics" && method == "GET") {
            std::ostringstream stats_json;
            stats_json << "{\n"
                       << "  \"total_capacity\": " << pool->total_capacity() << ",\n"
                       << "  \"free_memory\": " << pool->total_free_memory() << ",\n"
                       << "  \"largest_free_block\": " << pool->largest_free_block() << ",\n"
                       << "  \"active_allocations\": " << pool->active_allocations() << "\n"
                       << "}";
            response_body = make_json_response(true, "Statistics", stats_json.str());
        } else if (path == "/api/pool/reset" && method == "POST") {
            pool->reset();
            active_allocations.clear();
            response_body = make_json_response(true, "Pool reset", "{\"reset\": true}");
        } else if (path == "/api/allocate" && method == "POST") {
            const size_t requested_size = extract_json_size(body, "size");
            if (requested_size == 0) {
                success = false;
                response_message = "Missing allocation size";
                response_body = make_json_response(false, response_message, "{}");
            } else {
                const uint64_t allocation_id = next_allocation_id_++;
                void* ptr = pool->allocate(requested_size, allocation_id);
                if (ptr == nullptr) {
                    success = false;
                    response_message = "Allocation failed";
                    response_body = make_json_response(false, response_message, "{}");
                } else {
                    active_allocations[allocation_id] = ptr;
                    response_body = make_json_response(true, "Allocation succeeded", "{\"allocation_id\": " + std::to_string(allocation_id) + ", \"state\": " + pool->to_json() + "}");
                }
            }
        } else if (path == "/api/free" && method == "POST") {
            const uint64_t allocation_id = extract_json_id(body, "allocation_id");
            auto it = active_allocations.find(allocation_id);
            if (it == active_allocations.end()) {
                success = false;
                response_message = "Allocation id not active";
                response_body = make_json_response(false, response_message, "{}");
            } else {
                const bool freed = pool->deallocate(it->second);
                if (!freed) {
                    success = false;
                    response_message = "Deallocation failed";
                    response_body = make_json_response(false, response_message, "{}");
                } else {
                    active_allocations.erase(it);
                    response_body = make_json_response(true, "Allocation freed", pool->to_json());
                }
            }
        } else if (path == "/api/workload/session/start" && method == "POST") {
            const std::vector<WorkloadEntry> workload = parse_workload_json(body);
            const std::string requested_strategy = to_lower(extract_json_value(body, "strategy"));
            const std::string strategy_name = normalize_algorithm_name(requested_strategy.empty() ? "first_fit" : requested_strategy);
            const std::string allocation_mode = normalize_allocation_mode(
                requested_strategy,
                extract_json_value(body, "allocation_mode"));
            const size_t fixed_block_size = extract_json_size(body, "fixed_block_size");
            if (workload.empty()) {
                response_body = make_json_response(false, "No valid workload operations found", "{}");
            } else if (!is_supported_strategy(requested_strategy.empty() ? "first_fit" : requested_strategy)
                || (allocation_mode != "fixed_size" && allocation_mode != "variable_size")
                || (allocation_mode == "fixed_size" && (fixed_block_size == 0 || fixed_block_size > pool->total_capacity()))) {
                response_body = make_json_response(false, "Invalid allocation strategy, mode, or fixed block size.", "{}");
            } else {
                std::lock_guard<std::mutex> lock(g_workload_session_mutex);
                g_workload_session = WorkloadSession{};
                g_workload_session.status = WorkloadSession::Status::Paused;
                g_workload_session.workload = workload;
                g_workload_session.strategy = strategy_name;
                g_workload_session.allocation_mode = allocation_mode;
                g_workload_session.fixed_block_size = allocation_mode == "fixed_size" ? fixed_block_size : 0;
                g_workload_session.capacity = pool->total_capacity();
                g_workload_session.experiment_id = generate_experiment_id();
                g_workload_session.pool = std::make_unique<MemoryPool>(
                    g_workload_session.capacity,
                    g_workload_session.fixed_block_size,
                    strategy_from_name(strategy_name),
                    allocation_mode_from_name(allocation_mode));
                response_body = make_json_response(true, "Workload session started", workload_session_json(g_workload_session));
            }
        } else if (path == "/api/workload/session/status" && method == "GET") {
            std::lock_guard<std::mutex> lock(g_workload_session_mutex);
            response_body = make_json_response(true, "Workload session status", workload_session_json(g_workload_session));
        } else if (path == "/api/workload/session/pause" && method == "POST") {
            std::lock_guard<std::mutex> lock(g_workload_session_mutex);
            if (g_workload_session.status == WorkloadSession::Status::Running) {
                g_workload_session.status = WorkloadSession::Status::Paused;
                response_body = make_json_response(true, "Workload paused", workload_session_json(g_workload_session));
            } else {
                response_body = make_json_response(false, "Only a running workload can be paused.", workload_session_json(g_workload_session));
            }
        } else if (path == "/api/workload/session/resume" && method == "POST") {
            std::lock_guard<std::mutex> lock(g_workload_session_mutex);
            if (g_workload_session.status == WorkloadSession::Status::Paused) {
                g_workload_session.status = WorkloadSession::Status::Running;
                response_body = make_json_response(true, "Workload resumed", workload_session_json(g_workload_session));
            } else {
                response_body = make_json_response(false, "Only a paused workload can resume.", workload_session_json(g_workload_session));
            }
        } else if (path == "/api/workload/session/step" && method == "POST") {
            std::lock_guard<std::mutex> lock(g_workload_session_mutex);
            if (!g_workload_session.pool || g_workload_session.next_operation >= g_workload_session.workload.size()) {
                response_body = make_json_response(false, "No active workload session.", workload_session_json(g_workload_session));
            } else if (g_workload_session.status != WorkloadSession::Status::Paused
                && g_workload_session.status != WorkloadSession::Status::Running) {
                response_body = make_json_response(false, "Workload is not ready for a step.", workload_session_json(g_workload_session));
            } else {
                g_workload_session.status = WorkloadSession::Status::Running;
                g_workload_session.results.push_back(execute_session_operation(g_workload_session));
                ++g_workload_session.next_operation;
                if (g_workload_session.next_operation == g_workload_session.workload.size()) {
                    ExperimentRecord completed;
                    if (!finalize_workload_session(g_workload_session, experiment_store, completed, store_error)) {
                        g_workload_session.status = WorkloadSession::Status::Failed;
                        response_body = make_json_response(false, "Workload completed but could not be saved: " + store_error, workload_session_json(g_workload_session));
                    } else {
                        {
                            std::lock_guard<std::mutex> history_lock(g_experiments_mutex);
                            g_experiments.push_back(completed);
                        }
                        g_workload_session.status = WorkloadSession::Status::Completed;
                        response_body = make_json_response(true, "Workload completed and saved", workload_session_json(g_workload_session));
                    }
                } else {
                    g_workload_session.status = WorkloadSession::Status::Paused;
                    response_body = make_json_response(true, "Workload operation completed", workload_session_json(g_workload_session));
                }
            }
        } else if (path == "/api/workload/session/reset" && method == "POST") {
            std::lock_guard<std::mutex> lock(g_workload_session_mutex);
            g_workload_session = WorkloadSession{};
            response_body = make_json_response(true, "Workload session reset", workload_session_json(g_workload_session));
        } else if (path == "/api/workload/execute" && method == "POST") {
            const std::vector<WorkloadEntry> workload = parse_workload(body);
            if (workload.empty()) {
                success = false;
                response_message = "No valid workload operations found";
                response_body = make_json_response(false, response_message, "{}");
            } else if (std::any_of(workload.begin(), workload.end(), [](const WorkloadEntry& entry) {
                return to_lower(entry.operation) != "allocate" && to_lower(entry.operation) != "free";
            })) {
                success = false;
                response_message = "Workloads may contain only allocate and free operations.";
                response_body = make_json_response(false, response_message, "{}");
            } else {
                const std::string requested_strategy = to_lower(extract_json_value(body, "strategy").empty()
                    ? "first_fit"
                    : extract_json_value(body, "strategy"));
                const std::string strategy_name = normalize_algorithm_name(requested_strategy);
                const std::string allocation_mode = normalize_allocation_mode(
                    requested_strategy,
                    extract_json_value(body, "allocation_mode"));
                const size_t fixed_block_size = extract_json_size(body, "fixed_block_size");
                if (!is_supported_strategy(requested_strategy)
                    || (allocation_mode != "fixed_size" && allocation_mode != "variable_size")
                    || (allocation_mode == "fixed_size"
                        && (fixed_block_size == 0 || fixed_block_size > pool->total_capacity()))) {
                    success = false;
                    response_message = "Invalid allocation strategy or fixed block size.";
                    response_body = make_json_response(false, response_message, "{}");
                } else {
                    ExperimentRecord experiment = run_workload_experiment(
                        strategy_name,
                        allocation_mode,
                        pool->total_capacity(),
                        workload,
                        fixed_block_size,
                        extract_json_id(body, "random_seed"),
                        "workload_experiment");
                    const bool persisted = persist_experiment(experiment_store, experiment, store_error);
                    if (!persisted) {
                        success = false;
                        response_message = "Workload executed but experiment could not be saved: " + store_error;
                        response_body = make_json_response(false, response_message, "{}");
                    } else {
                        std::ostringstream payload;
                        payload << "{\n";
                        payload << "  \"experiment_id\": \"" << json_escape(experiment.experiment_id) << "\",\n";
                        payload << "  \"configuration\": {\"strategy\": \"" << json_escape(experiment.strategy)
                            << "\", \"allocation_mode\": \"" << json_escape(experiment.allocation_mode)
                            << "\", \"fixed_block_size\": " << experiment.fixed_block_size << "},\n";
                        payload << "  \"results\": [\n";
                        for (size_t i = 0; i < experiment.operation_sequence.size(); ++i) {
                            payload << operation_record_to_json(experiment.operation_sequence[i]);
                            if (i + 1 < experiment.operation_sequence.size()) {
                                payload << ",\n";
                            }
                        }
                        payload << "\n  ],\n";
                        payload << "  \"state\": " << experiment.final_memory_state_json << ",\n";
                        payload << "  \"metrics\": {\n";
                        payload << "    \"peak_memory_utilization\": " << experiment.memory_metrics.peak_memory_utilization << ",\n";
                        payload << "    \"average_allocation_latency_ms\": " << experiment.allocation_metrics.average_latency_ms << ",\n";
                        payload << "    \"p95_allocation_latency_ms\": " << experiment.allocation_metrics.p95_latency_ms << ",\n";
                        payload << "    \"success_rate\": " << experiment.allocation_metrics.success_rate << "\n";
                        payload << "  }\n";
                        payload << "}\n";
                        response_message = "Workload executed and saved";
                        response_body = make_json_response(true, response_message, payload.str());
                    }
                }
            }
        } else if (path == "/api/workload/random" && method == "POST") {
            const size_t operation_count = extract_json_size(body, "operation_count");
            const size_t minimum_size = extract_json_size(body, "minimum_size");
            const size_t maximum_size = extract_json_size(body, "maximum_size");
            const size_t allocation_probability = extract_json_size(body, "allocation_probability");
            const size_t deallocation_probability = extract_json_size(body, "deallocation_probability");
            const uint64_t random_seed = extract_json_id(body, "random_seed");
            const bool valid_seed = is_valid_uint64(trim(extract_json_value(body, "random_seed")));
            const std::string strategy_name = to_lower(extract_json_value(body, "strategy"));
            const std::string allocation_mode = to_lower(extract_json_value(body, "allocation_mode"));
            const size_t fixed_block_size = extract_json_size(body, "fixed_block_size");
            std::vector<WorkloadEntry> workload;
            if (!valid_seed) {
                success = false;
                response_message = "Random seed must be provided as a non-negative integer.";
                response_body = make_json_response(false, response_message, "{}");
            } else if (!generate_random_workload(
                    pool->total_capacity(),
                    operation_count,
                    minimum_size,
                    maximum_size,
                    allocation_probability,
                    deallocation_probability,
                    random_seed,
                    strategy_name,
                    allocation_mode,
                    fixed_block_size,
                    workload,
                    response_message)) {
                success = false;
                response_body = make_json_response(false, response_message, "{}");
            } else {
                ExperimentRecord experiment = run_workload_experiment(
                    strategy_name,
                    allocation_mode,
                    pool->total_capacity(),
                    workload,
                    fixed_block_size,
                    random_seed,
                    "Seeded random workload");
                experiment.workload_configuration =
                    "Seeded random: operations=" + std::to_string(operation_count)
                    + ", minimum_size_bytes=" + std::to_string(minimum_size)
                    + ", maximum_size_bytes=" + std::to_string(maximum_size)
                    + ", allocation_probability_percent=" + std::to_string(allocation_probability)
                    + ", deallocation_probability_percent=" + std::to_string(deallocation_probability);
                experiment.benchmark_results = compare_experiments({experiment});
                if (!persist_experiment(experiment_store, experiment, store_error)) {
                    success = false;
                    response_message = "Random workload executed but experiment could not be saved: " + store_error;
                    response_body = make_json_response(false, response_message, "{}");
                } else {
                    std::ostringstream payload;
                    payload << "{\n";
                    payload << "  \"experiment_id\": \"" << json_escape(experiment.experiment_id) << "\",\n";
                    payload << "  \"configuration\": {\"strategy\": \"" << json_escape(experiment.strategy)
                        << "\", \"allocation_mode\": \"" << json_escape(experiment.allocation_mode)
                        << "\", \"fixed_block_size\": " << experiment.fixed_block_size
                        << ", \"operation_count\": " << operation_count
                        << ", \"minimum_size\": " << minimum_size
                        << ", \"maximum_size\": " << maximum_size
                        << ", \"allocation_probability\": " << allocation_probability
                        << ", \"deallocation_probability\": " << deallocation_probability
                        << ", \"random_seed\": " << random_seed << "},\n";
                    payload << "  \"results\": [\n";
                    for (size_t i = 0; i < experiment.operation_sequence.size(); ++i) {
                        payload << operation_record_to_json(experiment.operation_sequence[i]);
                        if (i + 1 < experiment.operation_sequence.size()) payload << ",\n";
                    }
                    payload << "\n  ],\n";
                    payload << "  \"benchmark_results\": [";
                    for (size_t i = 0; i < experiment.benchmark_results.size(); ++i) {
                        if (i > 0) payload << ",";
                        payload << benchmark_result_to_json(experiment.benchmark_results[i]);
                    }
                    payload << "],\n";
                    payload << "  \"state\": " << experiment.final_memory_state_json << "\n";
                    payload << "}\n";
                    response_message = "Seeded random workload executed and saved";
                    response_body = make_json_response(true, response_message, payload.str());
                }
            }
        } else if (path == "/api/benchmark" && method == "POST") {
            const size_t capacity = extract_json_size(body, "capacity") == 0 ? pool->total_capacity() : extract_json_size(body, "capacity");
            const size_t fixed_block_size = extract_json_size(body, "fixed_block_size") == 0
                ? 64
                : extract_json_size(body, "fixed_block_size");
            std::vector<std::string> strategies;
            const std::string strategies_json = body;
            const std::string first_fit = "\"first_fit\"";
            const std::string best_fit = "\"best_fit\"";
            const std::string fixed_size = "\"fixed_size\"";
            const std::string variable_size = "\"variable_size\"";
            if (strategies_json.find(first_fit) != std::string::npos) strategies.push_back("first_fit");
            if (strategies_json.find(best_fit) != std::string::npos) strategies.push_back("best_fit");
            if (strategies_json.find(fixed_size) != std::string::npos) strategies.push_back("fixed_size");
            if (strategies_json.find(variable_size) != std::string::npos) strategies.push_back("variable_size");
            if (strategies.empty()) {
                strategies = {"first_fit", "best_fit"};
            }
            std::vector<size_t> sizes;
            const std::string sizes_json = body;
            const auto sizes_start = sizes_json.find("\"sizes\"");
            if (sizes_start != std::string::npos) {
                const auto array_start = sizes_json.find('[', sizes_start);
                const auto array_end = sizes_json.find(']', array_start);
                if (array_start != std::string::npos && array_end != std::string::npos) {
                    const std::string array_data = sizes_json.substr(array_start + 1, array_end - array_start - 1);
                    size_t cursor = 0;
                    while (cursor < array_data.size()) {
                        while (cursor < array_data.size() && !std::isdigit(static_cast<unsigned char>(array_data[cursor]))) {
                            ++cursor;
                        }
                        if (cursor >= array_data.size()) {
                            break;
                        }
                        const auto num_begin = cursor;
                        while (cursor < array_data.size() && std::isdigit(static_cast<unsigned char>(array_data[cursor]))) {
                            ++cursor;
                        }
                        const std::string size_text = array_data.substr(num_begin, cursor - num_begin);
                        try {
                            sizes.push_back(static_cast<size_t>(std::stoull(size_text)));
                        } catch (...) {
                        }
                    }
                }
            }
            if (sizes.empty()) {
                sizes = {32, 64, 128, 256, 512};
            }
            const size_t iterations = extract_json_size(body, "iterations") == 0 ? 1 : extract_json_size(body, "iterations");
            constexpr size_t kMaximumBenchmarkSamples = 100000;
            if (capacity == 0 || capacity > 64 * 1024 * 1024 || sizes.size() > kMaximumBenchmarkSamples
                || iterations == 0 || iterations > kMaximumBenchmarkSamples
                || (sizes.size() != 0 && iterations > kMaximumBenchmarkSamples / sizes.size())
                || fixed_block_size == 0 || fixed_block_size > capacity) {
                success = false;
                response_message = "Invalid benchmark configuration or sample count exceeds 100000.";
                response_body = make_json_response(false, response_message, "{}");
            } else {
                std::vector<AlgorithmBenchmarkResult> comparisons = compare_allocator_algorithms(
                    capacity, sizes, strategies, iterations, fixed_block_size, 0);
                std::vector<WorkloadEntry> benchmark_workload;
                benchmark_workload.reserve(sizes.size());
                for (size_t i = 0; i < sizes.size(); ++i) {
                    benchmark_workload.push_back({"allocate", sizes[i], static_cast<uint64_t>(i + 1)});
                }
                bool all_results_saved = true;
                for (auto& comparison : comparisons) {
                    ExperimentRecord experiment = run_workload_experiment(
                        comparison.strategy,
                        comparison.strategy == "fixed_size" ? "fixed_size" : "variable_size",
                        capacity,
                        benchmark_workload,
                        fixed_block_size,
                        0,
                        "Benchmark " + comparison.strategy);
                    comparison.experiment_id = experiment.experiment_id;
                    comparison.name = experiment.name;
                    experiment.benchmark_results.push_back(comparison);
                    std::lock_guard<std::mutex> lock(g_experiments_mutex);
                    if (!experiment_store.append(experiment, store_error)) {
                        all_results_saved = false;
                        break;
                    }
                    g_experiments.push_back(std::move(experiment));
                }
                if (!all_results_saved) {
                    success = false;
                    response_message = "Benchmark ran but experiment history could not be fully saved: " + store_error;
                    response_body = make_json_response(false, response_message, "{}");
                } else {
                    std::ostringstream payload;
                    payload << "{\n";
                    payload << "  \"results\": [\n";
                    for (size_t i = 0; i < comparisons.size(); ++i) {
                        payload << benchmark_result_to_json(comparisons[i]);
                        if (i + 1 < comparisons.size()) {
                            payload << ",\n";
                        }
                    }
                    payload << "\n  ]\n";
                    payload << "}\n";
                    response_body = make_json_response(true, "Benchmark results saved to experiment history", payload.str());
                }
            }
        } else if (path == "/api/experiments" && method == "DELETE") {
            ExperimentStore experiment_store(experiment_store_path());
            std::string store_error;
            std::lock_guard<std::mutex> lock(g_experiments_mutex);
            if (!experiment_store.clear(store_error)) {
                response_body = make_json_response(false, "Unable to clear experiment history: " + store_error, "{}");
            } else {
                g_experiments.clear();
                response_body = make_json_response(true, "Experiment history cleared", "{\"experiments\": []}");
            }
        } else if (path == "/api/experiments" && method == "GET") {
            std::lock_guard<std::mutex> lock(g_experiments_mutex);
            std::ostringstream payload;
            payload << "{\n";
            payload << "  \"experiments\": [\n";
            for (size_t i = 0; i < g_experiments.size(); ++i) {
                payload << experiment_summary_to_json(g_experiments[i]);
                if (i + 1 < g_experiments.size()) {
                    payload << ",\n";
                }
            }
            payload << "\n  ]\n";
            payload << "}\n";
            response_body = make_json_response(true, "Experiments", payload.str());
        } else if (path.find("/api/experiments/detail") == 0 && method == "GET") {
            std::string id = parse_query_value(path, "id");
            ExperimentRecord record;
            std::lock_guard<std::mutex> lock(g_experiments_mutex);
            for (const auto& item : g_experiments) {
                if (item.experiment_id == id) {
                    record = item;
                    break;
                }
            }
            if (record.experiment_id.empty()) {
                response_body = make_json_response(false, "Experiment not found", "{}");
            } else {
                response_body = make_json_response(true, "Experiment detail", experiment_record_to_json(record));
            }
        } else if (path.find("/api/experiments/export") == 0 && method == "GET") {
            std::string id = parse_query_value(path, "id");
            if (id.empty() && !g_experiments.empty()) {
                id = g_experiments.back().experiment_id;
            }
            std::string format = parse_query_value(path, "format");
            if (format.empty()) format = "full";
            ExperimentRecord record;
            std::lock_guard<std::mutex> lock(g_experiments_mutex);
            for (const auto& item : g_experiments) {
                if (item.experiment_id == id) {
                    record = item;
                    break;
                }
            }
            if (record.experiment_id.empty()) {
                response_body = make_json_response(false, "Experiment not found", "{}");
            } else if (format != "full" && format != "summary" && format != "operations"
                && format != "configuration" && format != "memory" && format != "fragmentation"
                && format != "benchmarks") {
                response_body = make_json_response(false, "Unsupported CSV export format", "{}");
            } else {
                response_body = make_json_response(true, "Experiment CSV export",
                    "{\"format\": \"" + format + "\", \"csv\": \""
                    + json_escape(export_experiment_csv(record, format)) + "\"}");
            }
        } else if (path == "/api/experiments/compare" && method == "POST") {
            const std::string ids_json = body;
            std::vector<std::string> ids;
            const auto ids_start = ids_json.find("\"experiment_ids\"");
            if (ids_start != std::string::npos) {
                const auto array_start = ids_json.find('[', ids_start);
                const auto array_end = ids_json.find(']', array_start);
                if (array_start != std::string::npos && array_end != std::string::npos) {
                    const std::string array_data = ids_json.substr(array_start + 1, array_end - array_start - 1);
                    std::string current;
                    for (char ch : array_data) {
                        if (ch == '"') {
                            continue;
                        }
                        if (ch == ',') {
                            if (!current.empty()) {
                                ids.push_back(current);
                                current.clear();
                            }
                        } else {
                            current.push_back(ch);
                        }
                    }
                    if (!current.empty()) {
                        ids.push_back(current);
                    }
                }
            }
            std::vector<ExperimentRecord> selected;
            {
                std::lock_guard<std::mutex> lock(g_experiments_mutex);
                if (ids.empty()) {
                    selected = g_experiments;
                } else {
                    selected = get_experiments_for_ids(ids);
                }
            }
            bool same_conditions = selected.size() > 1;
            std::vector<std::string> condition_differences;
            if (!selected.empty()) {
                const auto& baseline = selected.front();
                for (size_t i = 1; i < selected.size(); ++i) {
                    const auto& current = selected[i];
                    if (current.pool_capacity != baseline.pool_capacity) {
                        same_conditions = false;
                        condition_differences.push_back("Pool capacities differ");
                    }
                    if (current.allocation_mode != baseline.allocation_mode) {
                        same_conditions = false;
                        condition_differences.push_back("Allocation modes differ");
                    }
                    if (current.fixed_block_size != baseline.fixed_block_size) {
                        same_conditions = false;
                        condition_differences.push_back("Fixed block sizes differ");
                    }
                    if (current.workload_signature != baseline.workload_signature) {
                        same_conditions = false;
                        condition_differences.push_back("Workload operation sequences differ");
                    }
                    if (current.workload_configuration != baseline.workload_configuration) {
                        same_conditions = false;
                        condition_differences.push_back("Workload settings differ");
                    }
                    if (current.random_seed != baseline.random_seed) {
                        same_conditions = false;
                        condition_differences.push_back("Random seeds differ");
                    }
                }
            }
            std::sort(condition_differences.begin(), condition_differences.end());
            condition_differences.erase(std::unique(condition_differences.begin(), condition_differences.end()), condition_differences.end());
            const std::vector<AlgorithmBenchmarkResult> comparison_rows = compare_experiments(selected);
            std::ostringstream payload;
            payload << "{\n";
            payload << "  \"same_conditions\": " << (same_conditions ? "true" : "false") << ",\n";
            payload << "  \"condition_differences\": [";
            for (size_t i = 0; i < condition_differences.size(); ++i) {
                payload << '"' << json_escape(condition_differences[i]) << '"';
                if (i + 1 < condition_differences.size()) payload << ',';
            }
            payload << "],\n";
            payload << "  \"comparison\": [\n";
            for (size_t i = 0; i < comparison_rows.size(); ++i) {
                payload << benchmark_result_to_json(comparison_rows[i]);
                if (i + 1 < comparison_rows.size()) {
                    payload << ",\n";
                }
            }
            payload << "\n  ],\n";
            payload << "  \"csv\": \"" << json_escape(export_comparison_csv(comparison_rows)) << "\"\n";
            payload << "}\n";
            response_body = make_json_response(true, "Experiment comparison", payload.str());
        } else {
            success = false;
            response_message = "Not found";
            response_body = make_json_response(false, response_message, "{}");
        }

        const std::string http_response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(response_body.size()) + "\r\n\r\n" + response_body;
        send(client_fd, http_response.c_str(), http_response.size(), 0);
        close(client_fd);
    }

    close(server_fd);
    return 0;
}
