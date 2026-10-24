#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "experiment_metrics.h"
#include "memory_pool.h"

struct BenchmarkResult {
    size_t sample_count = 0;
    double allocation_latency_ns = 0.0;
    double deallocation_latency_ns = 0.0;
    double total_execution_time_ms = 0.0;
    double total_allocation_time_ms = 0.0;
    double total_deallocation_time_ms = 0.0;
    double maximum_allocation_latency_ms = 0.0;
    double maximum_deallocation_latency_ms = 0.0;
    double throughput_allocations_per_second = 0.0;
    double memory_utilization = 0.0;
    double peak_memory_utilization = 0.0;
    double final_memory_utilization = 0.0;
    double internal_fragmentation = 0.0;
    double internal_fragmentation_percent = 0.0;
    double external_fragmentation = 0.0;
    double allocation_success_rate = 0.0;
    double allocation_failure_rate = 0.0;
    size_t successful_allocations = 0;
    size_t failed_allocations = 0;
    size_t deallocation_requests = 0;
    size_t successful_deallocations = 0;
    size_t failed_deallocations = 0;
    size_t largest_free_block = 0;
    size_t free_block_count = 0;
    size_t block_splits = 0;
    size_t coalescing_operations = 0;
    size_t peak_memory_usage = 0;
    double p95_latency_ms = 0.0;
    double p99_latency_ms = 0.0;
};

BenchmarkResult benchmark_pool(
    size_t pool_capacity,
    const std::vector<size_t>& request_sizes,
    MemoryPool::Strategy strategy,
    size_t iterations = 1,
    size_t fixed_block_size = 64);

std::vector<AlgorithmBenchmarkResult> compare_allocator_algorithms(
    size_t pool_capacity,
    const std::vector<size_t>& request_sizes,
    const std::vector<std::string>& strategies,
    size_t iterations = 1,
    size_t fixed_block_size = 64,
    uint64_t random_seed = 0);
