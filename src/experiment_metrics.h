#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct OperationRecord {
    std::string operation;
    uint64_t allocation_id = 0;
    size_t requested_size = 0;
    size_t actual_size = 0;
    size_t offset = 0;
    bool success = false;
    double latency_ms = 0.0;
    std::string error_message;
};

struct AllocationLatencyStats {
    size_t total_requests = 0;
    size_t successful_requests = 0;
    size_t failed_requests = 0;
    double total_time_ms = 0.0;
    double average_latency_ms = 0.0;
    double median_latency_ms = 0.0;
    double min_latency_ms = 0.0;
    double max_latency_ms = 0.0;
    double p95_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
    double success_rate = 0.0;
    double failure_rate = 0.0;
};

struct MemoryMetricsSnapshot {
    size_t total_capacity = 0;
    size_t total_requested_memory = 0;
    size_t total_allocated_memory = 0;
    size_t total_free_memory = 0;
    size_t peak_memory_usage = 0;
    double average_memory_utilization = 0.0;
    double peak_memory_utilization = 0.0;
};

struct FragmentationMetrics {
    double internal_fragmentation_bytes = 0.0;
    double internal_fragmentation_percent = 0.0;
    double external_fragmentation_ratio = 0.0;
    size_t total_free_blocks = 0;
    size_t largest_free_block = 0;
    double average_free_block_size = 0.0;
    size_t contiguous_memory_failures = 0;
    size_t successful_coalescing_operations = 0;
    size_t block_splitting_operations = 0;
};

struct WorkloadMetrics {
    size_t total_operations = 0;
    size_t allocation_operations = 0;
    size_t deallocation_operations = 0;
    size_t successful_operations = 0;
    size_t failed_operations = 0;
    double workload_execution_duration_ms = 0.0;
    std::vector<std::pair<size_t, size_t>> allocation_size_distribution;
    size_t peak_active_allocations = 0;
    size_t total_memory_requested_throughout_workload = 0;
};

struct AlgorithmBenchmarkResult {
    std::string experiment_id;
    std::string name;
    std::string strategy;
    std::string allocation_mode;
    size_t allocation_requests = 0;
    size_t successful_allocations = 0;
    size_t failed_allocations = 0;
    size_t deallocation_requests = 0;
    size_t successful_deallocations = 0;
    size_t failed_deallocations = 0;
    double total_allocation_time_ms = 0.0;
    double average_deallocation_latency_ms = 0.0;
    double maximum_allocation_latency_ms = 0.0;
    double total_deallocation_time_ms = 0.0;
    double maximum_deallocation_latency_ms = 0.0;
    double execution_time_ms = 0.0;
    double average_allocation_latency_ms = 0.0;
    double p95_allocation_latency_ms = 0.0;
    double p99_allocation_latency_ms = 0.0;
    double allocation_success_rate = 0.0;
    double allocation_failure_rate = 0.0;
    double peak_memory_utilization = 0.0;
    size_t peak_memory_usage = 0;
    double final_memory_utilization = 0.0;
    double internal_fragmentation = 0.0;
    double external_fragmentation = 0.0;
    size_t largest_free_block = 0;
    size_t free_block_count = 0;
    size_t block_splits = 0;
    size_t coalescing_operations = 0;
};

struct ExperimentRecord {
    std::string experiment_id;
    std::string name;
    std::string timestamp;
    size_t pool_capacity = 0;
    std::string strategy;
    std::string allocation_mode;
    size_t fixed_block_size = 0;
    std::string workload_configuration;
    std::string workload_signature;
    uint64_t random_seed = 0;
    std::vector<OperationRecord> operation_sequence;
    MemoryMetricsSnapshot memory_metrics;
    AllocationLatencyStats allocation_metrics;
    AllocationLatencyStats deallocation_metrics;
    FragmentationMetrics fragmentation;
    WorkloadMetrics workload_metrics;
    std::vector<AlgorithmBenchmarkResult> benchmark_results;
    std::string final_memory_state_json;
    std::vector<std::string> errors;
};

std::string generate_experiment_id();
std::string iso_timestamp_now();
std::vector<double> percentile_values(const std::vector<double>& values, double percentile);
std::string csv_escape(const std::string& input);
std::string rows_to_csv(const std::vector<std::vector<std::string>>& rows);
std::vector<std::pair<size_t, size_t>> build_size_distribution(const std::vector<size_t>& sizes);
std::string export_experiment_csv(const ExperimentRecord& record, const std::string& section = "summary");
std::string export_comparison_csv(const std::vector<AlgorithmBenchmarkResult>& results);
std::vector<AlgorithmBenchmarkResult> compare_experiments(const std::vector<ExperimentRecord>& experiments);
