#include "experiment_store.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

constexpr char kFileHeader[] = "MEMORY_POOL_EXPERIMENTS_V1\n";
constexpr std::uint64_t kMaximumRecordBytes = 64 * 1024 * 1024;
constexpr std::uint64_t kMaximumCollectionItems = 1'000'000;

template <typename T>
void write_value(std::ostream& output, const T& value) {
    output << value << ' ';
}

void write_value(std::ostream& output, const std::string& value) {
    output << std::quoted(value) << ' ';
}

template <typename T>
bool read_value(std::istream& input, T& value) {
    return static_cast<bool>(input >> value);
}

bool read_value(std::istream& input, std::string& value) {
    return static_cast<bool>(input >> std::quoted(value));
}

template <typename T>
bool read_count(std::istream& input, T& count) {
    return read_value(input, count) && count <= kMaximumCollectionItems;
}

void write_latency_stats(std::ostream& output, const AllocationLatencyStats& stats) {
    write_value(output, stats.total_requests);
    write_value(output, stats.successful_requests);
    write_value(output, stats.failed_requests);
    write_value(output, stats.total_time_ms);
    write_value(output, stats.average_latency_ms);
    write_value(output, stats.median_latency_ms);
    write_value(output, stats.min_latency_ms);
    write_value(output, stats.max_latency_ms);
    write_value(output, stats.p95_latency_ms);
    write_value(output, stats.p99_latency_ms);
    write_value(output, stats.success_rate);
    write_value(output, stats.failure_rate);
}

bool read_latency_stats(std::istream& input, AllocationLatencyStats& stats) {
    return read_value(input, stats.total_requests)
        && read_value(input, stats.successful_requests)
        && read_value(input, stats.failed_requests)
        && read_value(input, stats.total_time_ms)
        && read_value(input, stats.average_latency_ms)
        && read_value(input, stats.median_latency_ms)
        && read_value(input, stats.min_latency_ms)
        && read_value(input, stats.max_latency_ms)
        && read_value(input, stats.p95_latency_ms)
        && read_value(input, stats.p99_latency_ms)
        && read_value(input, stats.success_rate)
        && read_value(input, stats.failure_rate);
}

void write_benchmark(std::ostream& output, const AlgorithmBenchmarkResult& result) {
    write_value(output, result.experiment_id);
    write_value(output, result.name);
    write_value(output, result.strategy);
    write_value(output, result.allocation_mode);
    write_value(output, result.allocation_requests);
    write_value(output, result.successful_allocations);
    write_value(output, result.failed_allocations);
    write_value(output, result.deallocation_requests);
    write_value(output, result.successful_deallocations);
    write_value(output, result.failed_deallocations);
    write_value(output, result.total_allocation_time_ms);
    write_value(output, result.average_deallocation_latency_ms);
    write_value(output, result.maximum_allocation_latency_ms);
    write_value(output, result.total_deallocation_time_ms);
    write_value(output, result.maximum_deallocation_latency_ms);
    write_value(output, result.execution_time_ms);
    write_value(output, result.average_allocation_latency_ms);
    write_value(output, result.p95_allocation_latency_ms);
    write_value(output, result.p99_allocation_latency_ms);
    write_value(output, result.allocation_success_rate);
    write_value(output, result.allocation_failure_rate);
    write_value(output, result.peak_memory_utilization);
    write_value(output, result.peak_memory_usage);
    write_value(output, result.final_memory_utilization);
    write_value(output, result.internal_fragmentation);
    write_value(output, result.external_fragmentation);
    write_value(output, result.largest_free_block);
    write_value(output, result.free_block_count);
    write_value(output, result.block_splits);
    write_value(output, result.coalescing_operations);
}

bool read_benchmark(std::istream& input, AlgorithmBenchmarkResult& result) {
    return read_value(input, result.experiment_id)
        && read_value(input, result.name)
        && read_value(input, result.strategy)
        && read_value(input, result.allocation_mode)
        && read_value(input, result.allocation_requests)
        && read_value(input, result.successful_allocations)
        && read_value(input, result.failed_allocations)
        && read_value(input, result.deallocation_requests)
        && read_value(input, result.successful_deallocations)
        && read_value(input, result.failed_deallocations)
        && read_value(input, result.total_allocation_time_ms)
        && read_value(input, result.average_deallocation_latency_ms)
        && read_value(input, result.maximum_allocation_latency_ms)
        && read_value(input, result.total_deallocation_time_ms)
        && read_value(input, result.maximum_deallocation_latency_ms)
        && read_value(input, result.execution_time_ms)
        && read_value(input, result.average_allocation_latency_ms)
        && read_value(input, result.p95_allocation_latency_ms)
        && read_value(input, result.p99_allocation_latency_ms)
        && read_value(input, result.allocation_success_rate)
        && read_value(input, result.allocation_failure_rate)
        && read_value(input, result.peak_memory_utilization)
        && read_value(input, result.peak_memory_usage)
        && read_value(input, result.final_memory_utilization)
        && read_value(input, result.internal_fragmentation)
        && read_value(input, result.external_fragmentation)
        && read_value(input, result.largest_free_block)
        && read_value(input, result.free_block_count)
        && read_value(input, result.block_splits)
        && read_value(input, result.coalescing_operations);
}

std::string serialize_record(const ExperimentRecord& record) {
    std::ostringstream output;
    write_value(output, record.experiment_id);
    write_value(output, record.name);
    write_value(output, record.timestamp);
    write_value(output, record.pool_capacity);
    write_value(output, record.strategy);
    write_value(output, record.allocation_mode);
    write_value(output, record.fixed_block_size);
    write_value(output, record.workload_configuration);
    write_value(output, record.workload_signature);
    write_value(output, record.random_seed);

    write_value(output, record.operation_sequence.size());
    for (const auto& operation : record.operation_sequence) {
        write_value(output, operation.operation);
        write_value(output, operation.allocation_id);
        write_value(output, operation.requested_size);
        write_value(output, operation.actual_size);
        write_value(output, operation.offset);
        write_value(output, operation.success);
        write_value(output, operation.latency_ms);
        write_value(output, operation.error_message);
    }

    write_value(output, record.memory_metrics.total_capacity);
    write_value(output, record.memory_metrics.total_requested_memory);
    write_value(output, record.memory_metrics.total_allocated_memory);
    write_value(output, record.memory_metrics.total_free_memory);
    write_value(output, record.memory_metrics.peak_memory_usage);
    write_value(output, record.memory_metrics.average_memory_utilization);
    write_value(output, record.memory_metrics.peak_memory_utilization);
    write_latency_stats(output, record.allocation_metrics);
    write_latency_stats(output, record.deallocation_metrics);

    write_value(output, record.fragmentation.internal_fragmentation_bytes);
    write_value(output, record.fragmentation.internal_fragmentation_percent);
    write_value(output, record.fragmentation.external_fragmentation_ratio);
    write_value(output, record.fragmentation.total_free_blocks);
    write_value(output, record.fragmentation.largest_free_block);
    write_value(output, record.fragmentation.average_free_block_size);
    write_value(output, record.fragmentation.contiguous_memory_failures);
    write_value(output, record.fragmentation.successful_coalescing_operations);
    write_value(output, record.fragmentation.block_splitting_operations);

    write_value(output, record.workload_metrics.total_operations);
    write_value(output, record.workload_metrics.allocation_operations);
    write_value(output, record.workload_metrics.deallocation_operations);
    write_value(output, record.workload_metrics.successful_operations);
    write_value(output, record.workload_metrics.failed_operations);
    write_value(output, record.workload_metrics.workload_execution_duration_ms);
    write_value(output, record.workload_metrics.allocation_size_distribution.size());
    for (const auto& entry : record.workload_metrics.allocation_size_distribution) {
        write_value(output, entry.first);
        write_value(output, entry.second);
    }
    write_value(output, record.workload_metrics.peak_active_allocations);
    write_value(output, record.workload_metrics.total_memory_requested_throughout_workload);

    write_value(output, record.benchmark_results.size());
    for (const auto& result : record.benchmark_results) {
        write_benchmark(output, result);
    }

    write_value(output, record.final_memory_state_json);
    write_value(output, record.errors.size());
    for (const auto& error : record.errors) {
        write_value(output, error);
    }
    return output.str();
}

bool deserialize_record(const std::string& serialized, ExperimentRecord& record) {
    std::istringstream input(serialized);
    std::uint64_t operation_count = 0;
    if (!read_value(input, record.experiment_id)
        || !read_value(input, record.name)
        || !read_value(input, record.timestamp)
        || !read_value(input, record.pool_capacity)
        || !read_value(input, record.strategy)
        || !read_value(input, record.allocation_mode)
        || !read_value(input, record.fixed_block_size)
        || !read_value(input, record.workload_configuration)
        || !read_value(input, record.workload_signature)
        || !read_value(input, record.random_seed)
        || !read_count(input, operation_count)) {
        return false;
    }

    record.operation_sequence.resize(static_cast<size_t>(operation_count));
    for (auto& operation : record.operation_sequence) {
        if (!read_value(input, operation.operation)
            || !read_value(input, operation.allocation_id)
            || !read_value(input, operation.requested_size)
            || !read_value(input, operation.actual_size)
            || !read_value(input, operation.offset)
            || !read_value(input, operation.success)
            || !read_value(input, operation.latency_ms)
            || !read_value(input, operation.error_message)) {
            return false;
        }
    }

    if (!read_value(input, record.memory_metrics.total_capacity)
        || !read_value(input, record.memory_metrics.total_requested_memory)
        || !read_value(input, record.memory_metrics.total_allocated_memory)
        || !read_value(input, record.memory_metrics.total_free_memory)
        || !read_value(input, record.memory_metrics.peak_memory_usage)
        || !read_value(input, record.memory_metrics.average_memory_utilization)
        || !read_value(input, record.memory_metrics.peak_memory_utilization)
        || !read_latency_stats(input, record.allocation_metrics)
        || !read_latency_stats(input, record.deallocation_metrics)
        || !read_value(input, record.fragmentation.internal_fragmentation_bytes)
        || !read_value(input, record.fragmentation.internal_fragmentation_percent)
        || !read_value(input, record.fragmentation.external_fragmentation_ratio)
        || !read_value(input, record.fragmentation.total_free_blocks)
        || !read_value(input, record.fragmentation.largest_free_block)
        || !read_value(input, record.fragmentation.average_free_block_size)
        || !read_value(input, record.fragmentation.contiguous_memory_failures)
        || !read_value(input, record.fragmentation.successful_coalescing_operations)
        || !read_value(input, record.fragmentation.block_splitting_operations)
        || !read_value(input, record.workload_metrics.total_operations)
        || !read_value(input, record.workload_metrics.allocation_operations)
        || !read_value(input, record.workload_metrics.deallocation_operations)
        || !read_value(input, record.workload_metrics.successful_operations)
        || !read_value(input, record.workload_metrics.failed_operations)
        || !read_value(input, record.workload_metrics.workload_execution_duration_ms)) {
        return false;
    }

    std::uint64_t distribution_count = 0;
    if (!read_count(input, distribution_count)) {
        return false;
    }
    record.workload_metrics.allocation_size_distribution.resize(static_cast<size_t>(distribution_count));
    for (auto& entry : record.workload_metrics.allocation_size_distribution) {
        if (!read_value(input, entry.first) || !read_value(input, entry.second)) {
            return false;
        }
    }
    if (!read_value(input, record.workload_metrics.peak_active_allocations)
        || !read_value(input, record.workload_metrics.total_memory_requested_throughout_workload)) {
        return false;
    }

    std::uint64_t benchmark_count = 0;
    if (!read_count(input, benchmark_count)) {
        return false;
    }
    record.benchmark_results.resize(static_cast<size_t>(benchmark_count));
    for (auto& result : record.benchmark_results) {
        if (!read_benchmark(input, result)) {
            return false;
        }
    }

    std::uint64_t error_count = 0;
    if (!read_value(input, record.final_memory_state_json) || !read_count(input, error_count)) {
        return false;
    }
    record.errors.resize(static_cast<size_t>(error_count));
    for (auto& error : record.errors) {
        if (!read_value(input, error)) {
            return false;
        }
    }

    input >> std::ws;
    return input.eof() && !record.experiment_id.empty();
}

}  // namespace

ExperimentStore::ExperimentStore(std::string path) : path_(std::move(path)) {}

bool ExperimentStore::load(std::vector<ExperimentRecord>& records, std::string& error) const {
    records.clear();
    std::ifstream input(path_, std::ios::binary);
    if (!input) {
        if (!std::filesystem::exists(path_)) {
            return true;
        }
        error = "Unable to open experiment history file: " + path_;
        return false;
    }

    std::string header;
    if (!std::getline(input, header) || header + '\n' != kFileHeader) {
        error = "Experiment history file has an unsupported or invalid format: " + path_;
        return false;
    }

    while (input.peek() != std::char_traits<char>::eof()) {
        std::string length_line;
        if (!std::getline(input, length_line)) {
            error = "Experiment history file ends unexpectedly: " + path_;
            return false;
        }
        std::uint64_t record_size = 0;
        try {
            size_t parsed = 0;
            record_size = std::stoull(length_line, &parsed);
            if (parsed != length_line.size()) {
                throw std::invalid_argument("invalid record length");
            }
        } catch (...) {
            error = "Experiment history contains an invalid record length: " + path_;
            return false;
        }
        if (record_size == 0 || record_size > kMaximumRecordBytes) {
            error = "Experiment history record length is outside supported limits: " + path_;
            return false;
        }

        std::string serialized(static_cast<size_t>(record_size), '\0');
        input.read(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        char delimiter = '\0';
        if (!input || !input.get(delimiter) || delimiter != '\n') {
            error = "Experiment history contains a truncated record: " + path_;
            return false;
        }
        ExperimentRecord record;
        if (!deserialize_record(serialized, record)) {
            error = "Experiment history contains an invalid record: " + path_;
            return false;
        }
        records.push_back(std::move(record));
    }
    return true;
}

bool ExperimentStore::append(const ExperimentRecord& record, std::string& error) const {
    const std::string serialized = serialize_record(record);
    if (serialized.empty() || serialized.size() > kMaximumRecordBytes) {
        error = "Experiment record exceeds the supported storage size";
        return false;
    }

    const std::filesystem::path file_path(path_);
    std::error_code filesystem_error;
    if (!file_path.parent_path().empty()) {
        std::filesystem::create_directories(file_path.parent_path(), filesystem_error);
        if (filesystem_error) {
            error = "Unable to create experiment history directory: " + filesystem_error.message();
            return false;
        }
    }

    const bool exists = std::filesystem::exists(file_path, filesystem_error);
    if (filesystem_error) {
        error = "Unable to inspect experiment history file: " + filesystem_error.message();
        return false;
    }
    if (!exists) {
        std::ofstream initialize(file_path, std::ios::binary | std::ios::out);
        if (!initialize) {
            error = "Unable to initialize experiment history file: " + path_;
            return false;
        }
        initialize.write(kFileHeader, sizeof(kFileHeader) - 1);
        initialize.flush();
        if (!initialize) {
            error = "Unable to initialize experiment history file: " + path_;
            return false;
        }
    }

    std::ofstream output(file_path, std::ios::binary | std::ios::out | std::ios::app);
    if (!output) {
        error = "Unable to open experiment history file for append: " + path_;
        return false;
    }
    output << serialized.size() << '\n';
    output.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
    output.put('\n');
    output.flush();
    if (!output) {
        error = "Failed to write experiment history record: " + path_;
        return false;
    }
    return true;
}

bool ExperimentStore::clear(std::string& error) const {
    const std::filesystem::path file_path(path_);
    std::error_code filesystem_error;
    if (!file_path.parent_path().empty()) {
        std::filesystem::create_directories(file_path.parent_path(), filesystem_error);
        if (filesystem_error) {
            error = "Unable to create experiment history directory: " + filesystem_error.message();
            return false;
        }
    }

    std::ofstream output(file_path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!output) {
        error = "Unable to clear experiment history file: " + path_;
        return false;
    }
    output.write(kFileHeader, sizeof(kFileHeader) - 1);
    output.flush();
    if (!output) {
        error = "Failed to clear experiment history file: " + path_;
        return false;
    }
    return true;
}

const std::string& ExperimentStore::path() const {
    return path_;
}
