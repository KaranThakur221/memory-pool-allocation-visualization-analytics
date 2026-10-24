#include "benchmark.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <vector>

namespace {

MemoryPool::Strategy strategy_from_name(const std::string& name) {
    if (name == "best_fit") return MemoryPool::Strategy::BestFit;
    if (name == "fixed_size") return MemoryPool::Strategy::FixedSize;
    return MemoryPool::Strategy::FirstFit;
}

}  // namespace

BenchmarkResult benchmark_pool(
    size_t pool_capacity,
    const std::vector<size_t>& request_sizes,
    MemoryPool::Strategy strategy,
    size_t iterations,
    size_t fixed_block_size) {
    BenchmarkResult result;
    if (iterations == 0 || request_sizes.empty()
        || request_sizes.size() > std::numeric_limits<size_t>::max() / iterations) {
        return result;
    }
    result.sample_count = request_sizes.size() * iterations;

    MemoryPool pool(pool_capacity, fixed_block_size, strategy);
    std::vector<double> allocation_latencies_ms;
    std::vector<void*> live_pointers;
    allocation_latencies_ms.reserve(result.sample_count);
    size_t successful = 0;
    size_t failed = 0;
    size_t successful_deallocations = 0;
    size_t failed_deallocations = 0;
    double total_allocation_ns = 0.0;
    double total_deallocation_ns = 0.0;
    double peak_internal_fragmentation_bytes = 0.0;
    double peak_internal_fragmentation_percent = 0.0;
    double peak_external_fragmentation = 0.0;

    const auto workload_start = std::chrono::steady_clock::now();
    for (size_t iteration = 0; iteration < iterations; ++iteration) {
        for (size_t size : request_sizes) {
            const uint64_t allocation_id = static_cast<uint64_t>(successful + failed + 1);
            const auto start = std::chrono::steady_clock::now();
            void* pointer = pool.allocate(size, allocation_id);
            const auto end = std::chrono::steady_clock::now();
            const double latency_ns = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
            const double latency_ms = latency_ns / 1'000'000.0;
            total_allocation_ns += latency_ns;
            allocation_latencies_ms.push_back(latency_ms);
            result.maximum_allocation_latency_ms = std::max(result.maximum_allocation_latency_ms, latency_ms);

            if (pointer == nullptr) {
                ++failed;
                continue;
            }
            ++successful;
            live_pointers.push_back(pointer);
            const MemoryStats snapshot = pool.stats();
            peak_internal_fragmentation_bytes = std::max(peak_internal_fragmentation_bytes, snapshot.internal_fragmentation);
            peak_internal_fragmentation_percent = std::max(peak_internal_fragmentation_percent, snapshot.internal_fragmentation_percent);
            peak_external_fragmentation = std::max(peak_external_fragmentation, snapshot.external_fragmentation);
        }

        for (void* pointer : live_pointers) {
            ++result.deallocation_requests;
            const auto start = std::chrono::steady_clock::now();
            const bool deallocated = pool.deallocate(pointer);
            const auto end = std::chrono::steady_clock::now();
            const double latency_ns = static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
            const double latency_ms = latency_ns / 1'000'000.0;
            total_deallocation_ns += latency_ns;
            result.maximum_deallocation_latency_ms = std::max(result.maximum_deallocation_latency_ms, latency_ms);
            if (deallocated) {
                ++successful_deallocations;
            } else {
                ++failed_deallocations;
            }
        }
        live_pointers.clear();
    }
    const auto workload_end = std::chrono::steady_clock::now();
    const MemoryStats final_stats = pool.stats();

    result.successful_allocations = successful;
    result.failed_allocations = failed;
    result.successful_deallocations = successful_deallocations;
    result.failed_deallocations = failed_deallocations;
    result.total_allocation_time_ms = total_allocation_ns / 1'000'000.0;
    result.total_deallocation_time_ms = total_deallocation_ns / 1'000'000.0;
    result.total_execution_time_ms = std::chrono::duration<double, std::milli>(workload_end - workload_start).count();
    result.allocation_latency_ns = total_allocation_ns / static_cast<double>(result.sample_count);
    result.deallocation_latency_ns = result.deallocation_requests == 0
        ? 0.0
        : total_deallocation_ns / static_cast<double>(result.deallocation_requests);
    result.memory_utilization = final_stats.memory_utilization;
    result.final_memory_utilization = final_stats.memory_utilization;
    result.peak_memory_utilization = pool_capacity == 0
        ? 0.0
        : static_cast<double>(final_stats.peak_memory_usage) / static_cast<double>(pool_capacity);
    result.internal_fragmentation = peak_internal_fragmentation_bytes;
    result.internal_fragmentation_percent = peak_internal_fragmentation_percent;
    result.external_fragmentation = peak_external_fragmentation;
    result.allocation_success_rate = static_cast<double>(successful) / static_cast<double>(result.sample_count);
    result.allocation_failure_rate = static_cast<double>(failed) / static_cast<double>(result.sample_count);
    result.throughput_allocations_per_second = result.total_execution_time_ms == 0.0
        ? 0.0
        : (static_cast<double>(successful) * 1000.0) / result.total_execution_time_ms;
    result.largest_free_block = final_stats.largest_free_block;
    result.free_block_count = final_stats.free_block_count;
    result.block_splits = final_stats.block_splitting_operations;
    result.coalescing_operations = final_stats.successful_coalescing_operations;
    result.peak_memory_usage = final_stats.peak_memory_usage;

    std::sort(allocation_latencies_ms.begin(), allocation_latencies_ms.end());
    const auto p95 = percentile_values(allocation_latencies_ms, 95.0);
    result.p95_latency_ms = p95.empty() ? 0.0 : p95.back();
    const auto p99 = percentile_values(allocation_latencies_ms, 99.0);
    result.p99_latency_ms = p99.empty() ? 0.0 : p99.back();
    return result;
}

std::vector<AlgorithmBenchmarkResult> compare_allocator_algorithms(
    size_t pool_capacity,
    const std::vector<size_t>& request_sizes,
    const std::vector<std::string>& strategies,
    size_t iterations,
    size_t fixed_block_size,
    uint64_t random_seed) {
    std::vector<AlgorithmBenchmarkResult> results;
    for (const auto& strategy_name : strategies) {
        const std::string normalized = strategy_name == "first_fit" ? "first_fit"
            : strategy_name == "best_fit" ? "best_fit"
            : strategy_name == "fixed_size" ? "fixed_size"
            : strategy_name == "variable_size" ? "variable_size"
            : strategy_name;
        const MemoryPool::Strategy strategy = strategy_from_name(normalized);
        const BenchmarkResult benchmark = benchmark_pool(
            pool_capacity, request_sizes, strategy, iterations, fixed_block_size);

        AlgorithmBenchmarkResult result;
        result.strategy = normalized;
        result.allocation_mode = normalized == "fixed_size" ? "fixed_size" : "variable_size";
        result.allocation_requests = benchmark.sample_count;
        result.successful_allocations = benchmark.successful_allocations;
        result.failed_allocations = benchmark.failed_allocations;
        result.deallocation_requests = benchmark.deallocation_requests;
        result.successful_deallocations = benchmark.successful_deallocations;
        result.failed_deallocations = benchmark.failed_deallocations;
        result.total_allocation_time_ms = benchmark.total_allocation_time_ms;
        result.average_deallocation_latency_ms = benchmark.deallocation_latency_ns / 1'000'000.0;
        result.maximum_allocation_latency_ms = benchmark.maximum_allocation_latency_ms;
        result.total_deallocation_time_ms = benchmark.total_deallocation_time_ms;
        result.maximum_deallocation_latency_ms = benchmark.maximum_deallocation_latency_ms;
        result.execution_time_ms = benchmark.total_execution_time_ms;
        result.average_allocation_latency_ms = benchmark.allocation_latency_ns / 1'000'000.0;
        result.p95_allocation_latency_ms = benchmark.p95_latency_ms;
        result.p99_allocation_latency_ms = benchmark.p99_latency_ms;
        result.allocation_success_rate = benchmark.allocation_success_rate;
        result.allocation_failure_rate = benchmark.allocation_failure_rate;
        result.peak_memory_utilization = benchmark.peak_memory_utilization * 100.0;
        result.peak_memory_usage = benchmark.peak_memory_usage;
        result.final_memory_utilization = benchmark.final_memory_utilization * 100.0;
        result.internal_fragmentation = benchmark.internal_fragmentation_percent;
        result.external_fragmentation = benchmark.external_fragmentation * 100.0;
        result.largest_free_block = benchmark.largest_free_block;
        result.free_block_count = benchmark.free_block_count;
        result.block_splits = benchmark.block_splits;
        result.coalescing_operations = benchmark.coalescing_operations;
        results.push_back(result);
    }
    (void)random_seed;
    return results;
}
