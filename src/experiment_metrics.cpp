#include "experiment_metrics.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

std::string generate_experiment_id() {
    static std::atomic<uint64_t> sequence{0};
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
    std::ostringstream out;
    out << "exp-" << timestamp << '-' << sequence.fetch_add(1, std::memory_order_relaxed);
    return out.str();
}

std::string iso_timestamp_now() {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
    gmtime_r(&time_t_now, &tm_now);
    std::ostringstream out;
    out << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%SZ");
    return out.str();
}

std::vector<double> percentile_values(const std::vector<double>& values, double percentile) {
    if (values.empty()) {
        return {};
    }
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    if (sorted.size() == 1) {
        return sorted;
    }
    const double safe_percentile = std::max(0.0, std::min(100.0, percentile));
    const double index = (sorted.size() - 1) * (safe_percentile / 100.0);
    const size_t lower = static_cast<size_t>(std::floor(index));
    const size_t upper = static_cast<size_t>(std::ceil(index));
    if (lower == upper) {
        return {sorted[lower]};
    }
    const double fraction = index - static_cast<double>(lower);
    return {sorted[lower] + (sorted[upper] - sorted[lower]) * fraction};
}

std::string csv_escape(const std::string& input) {
    if (input.empty()) {
        return "";
    }
    std::string escaped = input;
    bool needs_quotes = false;
    for (char c : input) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return escaped;
    }
    std::string output;
    output.reserve(input.size() + 2);
    output.push_back('"');
    for (char c : input) {
        if (c == '"') {
            output += "\"\"";
        } else {
            output.push_back(c);
        }
    }
    output.push_back('"');
    return output;
}

std::string rows_to_csv(const std::vector<std::vector<std::string>>& rows) {
    std::ostringstream out;
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            out << csv_escape(row[i]);
        }
        out << '\n';
    }
    return out.str();
}

std::vector<std::pair<size_t, size_t>> build_size_distribution(const std::vector<size_t>& sizes) {
    std::vector<std::pair<size_t, size_t>> distribution;
    if (sizes.empty()) {
        return distribution;
    }
    std::vector<size_t> unique = sizes;
    std::sort(unique.begin(), unique.end());
    for (size_t i = 0; i < unique.size(); ++i) {
        const size_t value = unique[i];
        size_t count = 1;
        while (i + 1 < unique.size() && unique[i + 1] == value) {
            ++count;
            ++i;
        }
        distribution.emplace_back(value, count);
    }
    return distribution;
}

std::string export_experiment_csv(const ExperimentRecord& record, const std::string& section) {
    std::vector<std::vector<std::string>> rows;
    if (section == "operations") {
        rows.push_back({"experiment_id", "operation_index", "operation", "allocation_id", "requested_size_bytes", "actual_size_bytes", "offset_bytes", "success", "latency_ms", "error"});
        for (size_t i = 0; i < record.operation_sequence.size(); ++i) {
            const auto& operation = record.operation_sequence[i];
            rows.push_back({record.experiment_id, std::to_string(i), operation.operation,
                std::to_string(operation.allocation_id), std::to_string(operation.requested_size),
                std::to_string(operation.actual_size), std::to_string(operation.offset),
                operation.success ? "true" : "false", std::to_string(operation.latency_ms),
                operation.error_message});
        }
    } else if (section == "configuration") {
        rows.push_back({"experiment_id", "name", "timestamp", "pool_capacity_bytes", "strategy", "allocation_mode", "fixed_block_size_bytes", "workload_configuration", "random_seed", "workload_signature"});
        rows.push_back({record.experiment_id, record.name, record.timestamp,
            std::to_string(record.pool_capacity), record.strategy, record.allocation_mode,
            std::to_string(record.fixed_block_size), record.workload_configuration,
            std::to_string(record.random_seed), record.workload_signature});
    } else if (section == "memory") {
        rows.push_back({"experiment_id", "total_capacity_bytes", "total_requested_bytes", "total_allocated_bytes", "total_free_bytes", "peak_memory_usage_bytes", "average_memory_utilization_percent", "peak_memory_utilization_percent"});
        rows.push_back({record.experiment_id, std::to_string(record.memory_metrics.total_capacity),
            std::to_string(record.memory_metrics.total_requested_memory),
            std::to_string(record.memory_metrics.total_allocated_memory),
            std::to_string(record.memory_metrics.total_free_memory),
            std::to_string(record.memory_metrics.peak_memory_usage),
            std::to_string(record.memory_metrics.average_memory_utilization),
            std::to_string(record.memory_metrics.peak_memory_utilization)});
    } else if (section == "fragmentation") {
        rows.push_back({"experiment_id", "internal_fragmentation_bytes", "internal_fragmentation_percent", "external_fragmentation_percent", "free_blocks", "largest_free_block_bytes", "average_free_block_bytes", "contiguous_memory_failures", "successful_coalesces", "block_splits"});
        rows.push_back({record.experiment_id,
            std::to_string(record.fragmentation.internal_fragmentation_bytes),
            std::to_string(record.fragmentation.internal_fragmentation_percent),
            std::to_string(record.fragmentation.external_fragmentation_ratio),
            std::to_string(record.fragmentation.total_free_blocks),
            std::to_string(record.fragmentation.largest_free_block),
            std::to_string(record.fragmentation.average_free_block_size),
            std::to_string(record.fragmentation.contiguous_memory_failures),
            std::to_string(record.fragmentation.successful_coalescing_operations),
            std::to_string(record.fragmentation.block_splitting_operations)});
    } else if (section == "benchmarks") {
        return export_comparison_csv(record.benchmark_results);
    } else if (section == "full") {
        rows.push_back({"experiment_id", "name", "timestamp", "pool_capacity_bytes", "strategy", "allocation_mode", "fixed_block_size_bytes", "random_seed", "workload_configuration", "workload_signature", "allocation_size_distribution_bytes_count", "operation_index", "operation", "allocation_id", "requested_size_bytes", "actual_size_bytes", "offset_bytes", "success", "operation_latency_ms", "operation_error", "total_requested_bytes", "total_allocated_bytes", "total_free_bytes", "peak_memory_usage_bytes", "average_memory_utilization_percent", "peak_memory_utilization_percent", "allocation_requests", "successful_allocations", "failed_allocations", "total_allocation_time_ms", "average_allocation_latency_ms", "median_allocation_latency_ms", "minimum_allocation_latency_ms", "maximum_allocation_latency_ms", "p95_allocation_latency_ms", "p99_allocation_latency_ms", "deallocation_requests", "successful_deallocations", "failed_deallocations", "total_deallocation_time_ms", "average_deallocation_latency_ms", "maximum_deallocation_latency_ms", "internal_fragmentation_bytes", "internal_fragmentation_percent", "external_fragmentation_percent", "free_blocks", "largest_free_block_bytes", "average_free_block_bytes", "contiguous_memory_failures", "successful_coalesces", "block_splits", "workload_operations", "successful_operations", "failed_operations", "workload_duration_ms", "peak_active_allocations", "total_memory_requested_bytes", "final_memory_state_json", "errors", "benchmark_results_csv"});
        std::string errors;
        std::string size_distribution;
        for (size_t i = 0; i < record.errors.size(); ++i) {
            if (i > 0) errors += "; ";
            errors += record.errors[i];
        }
        for (const auto& [size, count] : record.workload_metrics.allocation_size_distribution) {
            if (!size_distribution.empty()) size_distribution += ';';
            size_distribution += std::to_string(size) + ':' + std::to_string(count);
        }
        const std::string benchmark_csv = export_comparison_csv(record.benchmark_results);
        const auto append_operation = [&](const OperationRecord* operation, size_t index) {
            rows.push_back({record.experiment_id, record.name, record.timestamp,
                std::to_string(record.pool_capacity), record.strategy, record.allocation_mode,
                std::to_string(record.fixed_block_size), std::to_string(record.random_seed),
                record.workload_configuration, record.workload_signature, size_distribution,
                operation ? std::to_string(index) : "",
                operation ? operation->operation : "", operation ? std::to_string(operation->allocation_id) : "",
                operation ? std::to_string(operation->requested_size) : "",
                operation ? std::to_string(operation->actual_size) : "",
                operation ? std::to_string(operation->offset) : "",
                operation ? (operation->success ? "true" : "false") : "",
                operation ? std::to_string(operation->latency_ms) : "",
                operation ? operation->error_message : "", std::to_string(record.memory_metrics.total_requested_memory),
                std::to_string(record.memory_metrics.total_allocated_memory), std::to_string(record.memory_metrics.total_free_memory),
                std::to_string(record.memory_metrics.peak_memory_usage), std::to_string(record.memory_metrics.average_memory_utilization),
                std::to_string(record.memory_metrics.peak_memory_utilization),
                std::to_string(record.allocation_metrics.total_requests), std::to_string(record.allocation_metrics.successful_requests),
                std::to_string(record.allocation_metrics.failed_requests), std::to_string(record.allocation_metrics.total_time_ms),
                std::to_string(record.allocation_metrics.average_latency_ms), std::to_string(record.allocation_metrics.median_latency_ms),
                std::to_string(record.allocation_metrics.min_latency_ms), std::to_string(record.allocation_metrics.max_latency_ms),
                std::to_string(record.allocation_metrics.p95_latency_ms), std::to_string(record.allocation_metrics.p99_latency_ms),
                std::to_string(record.deallocation_metrics.total_requests), std::to_string(record.deallocation_metrics.successful_requests),
                std::to_string(record.deallocation_metrics.failed_requests), std::to_string(record.deallocation_metrics.total_time_ms),
                std::to_string(record.deallocation_metrics.average_latency_ms), std::to_string(record.deallocation_metrics.max_latency_ms),
                std::to_string(record.fragmentation.internal_fragmentation_bytes), std::to_string(record.fragmentation.internal_fragmentation_percent),
                std::to_string(record.fragmentation.external_fragmentation_ratio), std::to_string(record.fragmentation.total_free_blocks),
                std::to_string(record.fragmentation.largest_free_block), std::to_string(record.fragmentation.average_free_block_size),
                std::to_string(record.fragmentation.contiguous_memory_failures),
                std::to_string(record.fragmentation.successful_coalescing_operations),
                std::to_string(record.fragmentation.block_splitting_operations),
                std::to_string(record.workload_metrics.total_operations),
                std::to_string(record.workload_metrics.successful_operations),
                std::to_string(record.workload_metrics.failed_operations),
                std::to_string(record.workload_metrics.workload_execution_duration_ms),
                std::to_string(record.workload_metrics.peak_active_allocations),
                std::to_string(record.workload_metrics.total_memory_requested_throughout_workload),
                record.final_memory_state_json, errors, benchmark_csv});
        };
        if (record.operation_sequence.empty()) append_operation(nullptr, 0);
        else {
            for (size_t i = 0; i < record.operation_sequence.size(); ++i) append_operation(&record.operation_sequence[i], i);
        }
    } else {
        rows.push_back({"experiment_id", "name", "timestamp", "strategy", "allocation_mode", "pool_capacity_bytes", "fixed_block_size_bytes", "successful_allocations", "failed_allocations", "average_allocation_latency_ms", "p95_allocation_latency_ms", "p99_allocation_latency_ms", "peak_memory_utilization_percent", "internal_fragmentation_percent", "external_fragmentation_percent"});
        rows.push_back({record.experiment_id, record.name, record.timestamp, record.strategy,
            record.allocation_mode, std::to_string(record.pool_capacity), std::to_string(record.fixed_block_size),
            std::to_string(record.allocation_metrics.successful_requests),
            std::to_string(record.allocation_metrics.failed_requests),
            std::to_string(record.allocation_metrics.average_latency_ms),
            std::to_string(record.allocation_metrics.p95_latency_ms),
            std::to_string(record.allocation_metrics.p99_latency_ms),
            std::to_string(record.memory_metrics.peak_memory_utilization),
            std::to_string(record.fragmentation.internal_fragmentation_percent),
            std::to_string(record.fragmentation.external_fragmentation_ratio)});
    }
    return rows_to_csv(rows);
}

std::string export_comparison_csv(const std::vector<AlgorithmBenchmarkResult>& results) {
    std::vector<std::vector<std::string>> rows;
    rows.push_back({"experiment_id", "name", "strategy", "allocation_mode", "execution_time_ms", "total_allocation_time_ms", "average_allocation_latency_ms", "maximum_allocation_latency_ms", "p95_allocation_latency_ms", "p99_allocation_latency_ms", "allocation_requests", "successful_allocations", "failed_allocations", "allocation_success_rate", "allocation_failure_rate", "deallocation_requests", "successful_deallocations", "failed_deallocations", "total_deallocation_time_ms", "average_deallocation_latency_ms", "maximum_deallocation_latency_ms", "peak_memory_usage_bytes", "peak_memory_utilization", "final_memory_utilization", "internal_fragmentation", "external_fragmentation", "largest_free_block", "free_block_count", "block_splits", "coalescing_operations"});
    for (const auto& result : results) {
        rows.push_back({
            result.experiment_id,
            result.name,
            result.strategy,
            result.allocation_mode,
            std::to_string(result.execution_time_ms),
            std::to_string(result.total_allocation_time_ms),
            std::to_string(result.average_allocation_latency_ms),
            std::to_string(result.maximum_allocation_latency_ms),
            std::to_string(result.p95_allocation_latency_ms),
            std::to_string(result.p99_allocation_latency_ms),
            std::to_string(result.allocation_requests),
            std::to_string(result.successful_allocations),
            std::to_string(result.failed_allocations),
            std::to_string(result.allocation_success_rate),
            std::to_string(result.allocation_failure_rate),
            std::to_string(result.deallocation_requests),
            std::to_string(result.successful_deallocations),
            std::to_string(result.failed_deallocations),
            std::to_string(result.total_deallocation_time_ms),
            std::to_string(result.average_deallocation_latency_ms),
            std::to_string(result.maximum_deallocation_latency_ms),
            std::to_string(result.peak_memory_usage),
            std::to_string(result.peak_memory_utilization),
            std::to_string(result.final_memory_utilization),
            std::to_string(result.internal_fragmentation),
            std::to_string(result.external_fragmentation),
            std::to_string(result.largest_free_block),
            std::to_string(result.free_block_count),
            std::to_string(result.block_splits),
            std::to_string(result.coalescing_operations)
        });
    }
    return rows_to_csv(rows);
}

std::vector<AlgorithmBenchmarkResult> compare_experiments(const std::vector<ExperimentRecord>& experiments) {
    std::vector<AlgorithmBenchmarkResult> aggregated;
    for (const auto& record : experiments) {
        AlgorithmBenchmarkResult result;
        result.experiment_id = record.experiment_id;
        result.name = record.name;
        result.strategy = record.strategy;
        result.allocation_mode = record.allocation_mode;
        result.allocation_requests = record.allocation_metrics.total_requests;
        result.successful_allocations = record.allocation_metrics.successful_requests;
        result.failed_allocations = record.allocation_metrics.failed_requests;
        result.deallocation_requests = record.deallocation_metrics.total_requests;
        result.successful_deallocations = record.deallocation_metrics.successful_requests;
        result.failed_deallocations = record.deallocation_metrics.failed_requests;
        result.total_allocation_time_ms = record.allocation_metrics.total_time_ms;
        result.average_deallocation_latency_ms = record.deallocation_metrics.average_latency_ms;
        result.maximum_allocation_latency_ms = record.allocation_metrics.max_latency_ms;
        result.total_deallocation_time_ms = record.deallocation_metrics.total_time_ms;
        result.maximum_deallocation_latency_ms = record.deallocation_metrics.max_latency_ms;
        result.execution_time_ms = record.workload_metrics.workload_execution_duration_ms;
        result.average_allocation_latency_ms = record.allocation_metrics.average_latency_ms;
        result.p95_allocation_latency_ms = record.allocation_metrics.p95_latency_ms;
        result.p99_allocation_latency_ms = record.allocation_metrics.p99_latency_ms;
        result.allocation_success_rate = record.allocation_metrics.success_rate;
        result.allocation_failure_rate = record.allocation_metrics.failure_rate;
        result.peak_memory_utilization = record.memory_metrics.peak_memory_utilization;
        result.peak_memory_usage = record.memory_metrics.peak_memory_usage;
        result.final_memory_utilization = record.memory_metrics.total_capacity == 0 ? 0.0 : (static_cast<double>(record.memory_metrics.total_allocated_memory) / static_cast<double>(record.memory_metrics.total_capacity)) * 100.0;
        result.internal_fragmentation = record.fragmentation.internal_fragmentation_percent;
        result.external_fragmentation = record.fragmentation.external_fragmentation_ratio;
        result.largest_free_block = record.fragmentation.largest_free_block;
        result.free_block_count = record.fragmentation.total_free_blocks;
        result.block_splits = record.fragmentation.block_splitting_operations;
        result.coalescing_operations = record.fragmentation.successful_coalescing_operations;
        aggregated.push_back(result);
    }
    return aggregated;
}
