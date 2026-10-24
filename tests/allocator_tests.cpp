#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

#include "benchmark.h"
#include "experiment_metrics.h"
#include "experiment_store.h"
#include "fixed_size_pool.h"
#include "memory_pool.h"
#include "variable_size_pool.h"

int main() {
    {
        assert(percentile_values({}, 95.0).empty());
        assert(percentile_values({4.0}, 99.0) == std::vector<double>{4.0});
        const auto median = percentile_values({4.0, 1.0, 3.0, 2.0}, 50.0);
        const auto p95 = percentile_values({4.0, 1.0, 3.0, 2.0}, 95.0);
        assert(median.size() == 1 && median.front() == 2.5);
        assert(p95.size() == 1 && std::abs(p95.front() - 3.85) < 1e-9);
        assert(percentile_values({4.0, 1.0, 3.0, 2.0}, 150.0).front() == 4.0);
    }

    {
        MemoryPool pool(1024, 64, MemoryPool::Strategy::FirstFit);
        void* ptr = pool.allocate(64, 1);
        assert(ptr != nullptr);
        assert(pool.active_allocations() == 1);
        assert(pool.deallocate(ptr));
    }

    {
        FixedSizePool fixed_pool(1024, 64);
        void* p1 = fixed_pool.allocate();
        void* p2 = fixed_pool.allocate();
        assert(p1 != nullptr);
        assert(p2 != nullptr);
        assert(fixed_pool.deallocate(p1));
        assert(fixed_pool.deallocate(p2));
    }

    {
        VariableSizePool variable_pool(1024, MemoryPool::Strategy::BestFit);
        void* first = variable_pool.allocate(32, 1);
        void* second = variable_pool.allocate(64, 2);
        assert(first != nullptr);
        assert(second != nullptr);
        assert(variable_pool.deallocate(first));
        assert(variable_pool.deallocate(second));
    }

    {
        MemoryPool pool(256, 32, MemoryPool::Strategy::BestFit);
        void* p1 = pool.allocate(32, 1);
        void* p2 = pool.allocate(64, 2);
        void* p3 = pool.allocate(96, 3);
        assert(p1 != nullptr);
        assert(p2 != nullptr);
        assert(p3 != nullptr);
        assert(pool.deallocate(p2));
        assert(pool.allocate(64, 4) != nullptr);
    }

    {
        MemoryPool pool(256, 32, MemoryPool::Strategy::FirstFit);
        auto* ptr = pool.allocate(512, 99);
        assert(ptr == nullptr);
        auto stats = pool.stats();
        assert(stats.failed_allocations == 1);
        assert(stats.successful_allocations == 0);
    }

    {
        MemoryPool pool(256, 32, MemoryPool::Strategy::FirstFit);
        auto* ptr = pool.allocate(64, 1);
        assert(ptr != nullptr);
        assert(pool.deallocate(ptr));
        assert(!pool.deallocate(ptr));
        pool.reset();
        auto stats = pool.stats();
        assert(stats.active_allocations == 0);
        assert(stats.free_memory == 256);
        assert(stats.total_capacity == 256);
    }

    {
        MemoryPool pool(128, 16, MemoryPool::Strategy::FirstFit);
        assert(pool.allocate(0, 1) == nullptr);
        assert(pool.allocate(1024, 2) == nullptr);
    }

    {
        MemoryPool pool(256, 64, MemoryPool::Strategy::FixedSize);
        auto* small_request = pool.allocate(37, 1);
        assert(small_request != nullptr);
        const auto allocated_stats = pool.stats();
        assert(allocated_stats.requested_memory == 37);
        assert(allocated_stats.allocated_memory == 64);
        assert(allocated_stats.internal_fragmentation == 27.0);
        assert(pool.allocate(65, 2) == nullptr);
        assert(pool.stats().failed_allocations == 1);
        assert(pool.deallocate(small_request));
    }

    {
        const BenchmarkResult fixed = benchmark_pool(
            256, {37, 65}, MemoryPool::Strategy::FixedSize, 1, 64);
        assert(fixed.sample_count == 2);
        assert(fixed.successful_allocations == 1);
        assert(fixed.failed_allocations == 1);
        assert(fixed.deallocation_requests == 1);
        assert(fixed.successful_deallocations == 1);
        assert(fixed.total_deallocation_time_ms > 0.0);
        assert(fixed.maximum_deallocation_latency_ms > 0.0);
        assert(fixed.peak_memory_usage == 64);
        assert(fixed.peak_memory_utilization == 0.25);
        assert(fixed.final_memory_utilization == 0.0);
        assert(fixed.internal_fragmentation_percent > 72.0);
        assert(fixed.total_allocation_time_ms > 0.0);
        assert(fixed.total_execution_time_ms >= fixed.total_allocation_time_ms);

        const auto comparison = compare_allocator_algorithms(
            256, {37, 65}, {"first_fit", "best_fit", "fixed_size", "variable_size"}, 2, 64, 42);
        assert(comparison.size() == 4);
        for (const auto& row : comparison) {
            assert(row.allocation_requests == 4);
            assert(row.execution_time_ms > 0.0);
            assert(row.total_allocation_time_ms > 0.0);
            assert(row.peak_memory_usage > 0);
            assert(row.peak_memory_utilization > row.final_memory_utilization);
        }
        assert(comparison[2].allocation_mode == "fixed_size");
        assert(comparison[2].failed_allocations == 2);
        assert(comparison[2].internal_fragmentation > 0.0);
    }

    {
        MemoryPool pool(128, 8, MemoryPool::Strategy::FirstFit);
        auto* ptr = pool.allocate(16, 1);
        assert(pool.deallocate(ptr));
        assert(!pool.deallocate(ptr));
    }

    {
        MemoryPool first_fit(512, 64, MemoryPool::Strategy::FirstFit);
        MemoryPool best_fit(512, 64, MemoryPool::Strategy::BestFit);
        std::vector<void*> first_fit_allocations;
        std::vector<void*> best_fit_allocations;
        const std::vector<size_t> sizes{128, 64, 96, 64, 160};
        for (size_t i = 0; i < sizes.size(); ++i) {
            first_fit_allocations.push_back(first_fit.allocate(sizes[i], i + 1));
            best_fit_allocations.push_back(best_fit.allocate(sizes[i], i + 1));
            assert(first_fit_allocations.back() != nullptr);
            assert(best_fit_allocations.back() != nullptr);
        }
        assert(first_fit.deallocate(first_fit_allocations[0]));
        assert(first_fit.deallocate(first_fit_allocations[2]));
        assert(best_fit.deallocate(best_fit_allocations[0]));
        assert(best_fit.deallocate(best_fit_allocations[2]));
        auto* first_fit_result = first_fit.allocate(80, 10);
        auto* best_fit_result = best_fit.allocate(80, 10);
        assert(first_fit_result != nullptr);
        assert(best_fit_result != nullptr);
        const auto first_fit_blocks = first_fit.blocks();
        const auto best_fit_blocks = best_fit.blocks();
        const auto first_fit_block = std::find_if(first_fit_blocks.begin(), first_fit_blocks.end(), [](const AllocationBlock& block) {
            return block.allocated && block.allocation_id == 10;
        });
        const auto best_fit_block = std::find_if(best_fit_blocks.begin(), best_fit_blocks.end(), [](const AllocationBlock& block) {
            return block.allocated && block.allocation_id == 10;
        });
        assert(first_fit_block != first_fit_blocks.end());
        assert(best_fit_block != best_fit_blocks.end());
        assert(first_fit_block->offset == 0);
        assert(best_fit_block->offset == 192);
    }

    {
        MemoryPool pool(256, 8, MemoryPool::Strategy::BestFit);
        auto* ptr1 = pool.allocate(40, 1);
        auto* ptr2 = pool.allocate(40, 2);
        auto* ptr3 = pool.allocate(40, 3);
        assert(ptr1 != nullptr && ptr2 != nullptr && ptr3 != nullptr);
        pool.deallocate(ptr2);
        pool.deallocate(ptr1);
        assert(pool.allocate(40, 4) != nullptr);
    }

    {
        MemoryPool pool(256, 8, MemoryPool::Strategy::FirstFit);
        auto* ptr = pool.allocate(37, 1);
        assert(ptr != nullptr);
        auto allocated_stats = pool.stats();
        assert(allocated_stats.requested_memory == 37);
        assert(allocated_stats.allocated_memory == 40);
        assert(allocated_stats.internal_fragmentation == 3.0);
        assert(allocated_stats.block_splitting_operations == 1);
        assert(pool.deallocate(ptr));

        const auto free_stats = pool.stats();
        assert(free_stats.peak_memory_usage == 40);
        assert(free_stats.peak_memory_utilization > 0.0);
        assert(free_stats.free_block_count == 1);
        assert(free_stats.largest_free_block == 256);
        assert(free_stats.successful_coalescing_operations == 1);
        assert(pool.allocate(256, 2) != nullptr);
    }

    {
        MemoryPool first_pool(128, 16, MemoryPool::Strategy::FirstFit);
        MemoryPool second_pool(128, 16, MemoryPool::Strategy::FirstFit);
        auto* allocation = first_pool.allocate(16, 1);
        assert(allocation != nullptr);
        assert(!second_pool.deallocate(allocation));
        assert(first_pool.active_allocations() == 1);
        assert(second_pool.active_allocations() == 0);
    }

    {
        MemoryPool pool(4096, 64, MemoryPool::Strategy::BestFit);
        std::mt19937 generator(20261024);
        std::vector<void*> allocations;
        uint64_t allocation_id = 1;
        for (size_t step = 0; step < 3000; ++step) {
            if (!allocations.empty() && generator() % 3 == 0) {
                const size_t index = generator() % allocations.size();
                assert(pool.deallocate(allocations[index]));
                allocations.erase(allocations.begin() + static_cast<std::ptrdiff_t>(index));
            } else {
                const size_t size = 1 + (generator() % 256);
                void* allocation = pool.allocate(size, allocation_id++);
                if (allocation != nullptr) allocations.push_back(allocation);
            }

            const auto blocks = pool.blocks();
            size_t total_size = 0;
            size_t previous_end = 0;
            bool first_block = true;
            for (const auto& block : blocks) {
                assert(block.size > 0);
                assert(block.offset <= 4096);
                assert(block.size <= 4096 - block.offset);
                if (!first_block) assert(block.offset >= previous_end);
                first_block = false;
                previous_end = block.offset + block.size;
                total_size += block.size;
            }
            assert(total_size == 4096);
            const auto stats = pool.stats();
            assert(stats.allocated_memory + stats.free_memory == 4096);
            assert(stats.active_allocations == allocations.size());
        }
        for (void* allocation : allocations) assert(pool.deallocate(allocation));
        const auto final_stats = pool.stats();
        assert(final_stats.free_memory == 4096);
        assert(final_stats.free_block_count == 1);
        assert(final_stats.largest_free_block == 4096);
    }

    {
        ExperimentRecord first;
        first.experiment_id = "exp-001";
        first.name = "First Fit Run";
        first.strategy = "first_fit";
        first.allocation_mode = "variable_size";
        first.pool_capacity = 1024;
        first.fixed_block_size = 64;
        first.allocation_metrics.successful_requests = 42;
        first.allocation_metrics.failed_requests = 3;
        first.allocation_metrics.total_requests = 45;
        first.allocation_metrics.total_time_ms = 19.0;
        first.allocation_metrics.max_latency_ms = 7.0;
        first.allocation_metrics.average_latency_ms = 2.5;
        first.allocation_metrics.p95_latency_ms = 6.0;
        first.allocation_metrics.p99_latency_ms = 8.8;
        first.allocation_metrics.success_rate = 0.93;
        first.allocation_metrics.failure_rate = 0.07;
        first.memory_metrics.peak_memory_utilization = 71.0;
        first.memory_metrics.total_allocated_memory = 768;
        first.memory_metrics.total_capacity = 1024;
        first.fragmentation.internal_fragmentation_percent = 12.5;
        first.fragmentation.external_fragmentation_ratio = 14.0;
        first.fragmentation.largest_free_block = 128;
        first.fragmentation.total_free_blocks = 6;
        first.fragmentation.block_splitting_operations = 5;
        first.fragmentation.successful_coalescing_operations = 2;
        first.workload_metrics.workload_execution_duration_ms = 42.0;
        first.deallocation_metrics.total_requests = 5;
        first.deallocation_metrics.successful_requests = 4;
        first.deallocation_metrics.failed_requests = 1;
        first.deallocation_metrics.average_latency_ms = 0.25;
        first.deallocation_metrics.total_time_ms = 1.0;
        first.deallocation_metrics.max_latency_ms = 0.5;
        first.workload_signature = "allocate:64:0;free:0:1;";
        first.memory_metrics.peak_memory_usage = 900;

        ExperimentRecord second;
        second.experiment_id = "exp-002";
        second.name = "Best Fit Run";
        second.strategy = "best_fit";
        second.allocation_mode = "variable_size";
        second.pool_capacity = 1024;
        second.fixed_block_size = 64;
        second.allocation_metrics.successful_requests = 45;
        second.allocation_metrics.failed_requests = 1;
        second.allocation_metrics.total_requests = 46;
        second.allocation_metrics.average_latency_ms = 1.8;
        second.allocation_metrics.p95_latency_ms = 4.4;
        second.allocation_metrics.p99_latency_ms = 5.2;
        second.allocation_metrics.success_rate = 0.98;
        second.allocation_metrics.failure_rate = 0.02;
        second.memory_metrics.peak_memory_utilization = 68.0;
        second.memory_metrics.total_allocated_memory = 720;
        second.memory_metrics.total_capacity = 1024;
        second.fragmentation.internal_fragmentation_percent = 10.25;
        second.fragmentation.external_fragmentation_ratio = 9.0;
        second.fragmentation.largest_free_block = 192;
        second.fragmentation.total_free_blocks = 4;
        second.fragmentation.block_splitting_operations = 3;
        second.fragmentation.successful_coalescing_operations = 4;
        second.workload_metrics.workload_execution_duration_ms = 36.0;
        second.deallocation_metrics.total_requests = 7;
        second.deallocation_metrics.successful_requests = 7;
        second.deallocation_metrics.average_latency_ms = 0.2;
        second.workload_signature = first.workload_signature;

        const auto comparisons = compare_experiments({first, second});
        assert(comparisons.size() == 2);
        assert(comparisons[0].strategy == "first_fit");
        assert(comparisons[1].strategy == "best_fit");
        assert(comparisons[0].allocation_success_rate > 0.0);
        assert(comparisons[0].allocation_requests == 45);
        assert(comparisons[0].total_allocation_time_ms == 19.0);
        assert(comparisons[0].maximum_allocation_latency_ms == 7.0);
        assert(comparisons[0].failed_allocations == 3);
        assert(comparisons[0].deallocation_requests == 5);
        assert(comparisons[0].total_deallocation_time_ms == 1.0);
        assert(comparisons[0].average_deallocation_latency_ms == 0.25);
        assert(comparisons[0].maximum_deallocation_latency_ms == 0.5);
        assert(comparisons[0].peak_memory_usage == 900);

        first.operation_sequence.push_back({"allocate", 9, 64, 64, 0, true, 0.125, ""});
        first.workload_metrics.allocation_size_distribution = {{64, 2}};
        const auto experiment_csv = export_experiment_csv(first);
        assert(experiment_csv.find("exp-001") != std::string::npos);
        assert(experiment_csv.find("First Fit Run") != std::string::npos);
        assert(export_experiment_csv(first, "operations").find("requested_size_bytes") != std::string::npos);
        assert(export_experiment_csv(first, "configuration").find("pool_capacity_bytes") != std::string::npos);
        assert(export_experiment_csv(first, "memory").find("total_requested_bytes") != std::string::npos);
        assert(export_experiment_csv(first, "fragmentation").find("largest_free_block_bytes") != std::string::npos);
        const auto full_csv = export_experiment_csv(first, "full");
        assert(full_csv.find("p99_allocation_latency_ms") != std::string::npos);
        assert(full_csv.find("64:2") != std::string::npos);

        const auto comparison_csv = export_comparison_csv(comparisons);
        assert(comparison_csv.find("first_fit") != std::string::npos);
        assert(comparison_csv.find("best_fit") != std::string::npos);
        assert(comparison_csv.find("average_deallocation_latency_ms") != std::string::npos);
        assert(comparison_csv.find("peak_memory_usage_bytes") != std::string::npos);
    }

    {
        const std::string first_id = generate_experiment_id();
        const std::string second_id = generate_experiment_id();
        assert(!first_id.empty());
        assert(first_id != second_id);
    }

    {
        const auto storage_path = std::filesystem::temp_directory_path() / "memory_pool_experiment_store_test.bin";
        std::error_code cleanup_error;
        std::filesystem::remove(storage_path, cleanup_error);
        ExperimentStore store(storage_path.string());
        ExperimentRecord original;
        original.experiment_id = "exp-persistence-test";
        original.name = "Persistent \"quoted\" experiment\nrecord";
        original.timestamp = "2026-10-24T12:00:00Z";
        original.pool_capacity = 4096;
        original.strategy = "best_fit";
        original.allocation_mode = "variable_size";
        original.fixed_block_size = 64;
        original.workload_configuration = "2 operations";
        original.workload_signature = "allocate:37:0;free:0:1;";
        original.random_seed = 123456;
        original.operation_sequence.push_back({"allocate", 1, 37, 40, 0, true, 0.125, ""});
        original.operation_sequence.push_back({"free", 1, 37, 40, 0, false, 0.25, "test failure note"});
        original.memory_metrics.total_capacity = 4096;
        original.memory_metrics.total_requested_memory = 37;
        original.memory_metrics.total_allocated_memory = 40;
        original.memory_metrics.total_free_memory = 4056;
        original.memory_metrics.peak_memory_usage = 40;
        original.memory_metrics.average_memory_utilization = 0.9765625;
        original.memory_metrics.peak_memory_utilization = 0.9765625;
        original.allocation_metrics.total_requests = 1;
        original.allocation_metrics.successful_requests = 1;
        original.allocation_metrics.total_time_ms = 0.125;
        original.allocation_metrics.average_latency_ms = 0.125;
        original.allocation_metrics.p95_latency_ms = 0.125;
        original.deallocation_metrics.total_requests = 1;
        original.deallocation_metrics.failed_requests = 1;
        original.fragmentation.internal_fragmentation_bytes = 3;
        original.fragmentation.internal_fragmentation_percent = 8.108108;
        original.fragmentation.external_fragmentation_ratio = 2.5;
        original.fragmentation.total_free_blocks = 1;
        original.fragmentation.largest_free_block = 4056;
        original.fragmentation.average_free_block_size = 4056.0;
        original.fragmentation.block_splitting_operations = 1;
        original.workload_metrics.total_operations = 2;
        original.workload_metrics.allocation_operations = 1;
        original.workload_metrics.deallocation_operations = 1;
        original.workload_metrics.successful_operations = 1;
        original.workload_metrics.failed_operations = 1;
        original.workload_metrics.workload_execution_duration_ms = 0.375;
        original.workload_metrics.allocation_size_distribution = {{37, 1}};
        original.workload_metrics.peak_active_allocations = 1;
        original.workload_metrics.total_memory_requested_throughout_workload = 37;
        AlgorithmBenchmarkResult benchmark;
        benchmark.experiment_id = original.experiment_id;
        benchmark.name = "actual benchmark";
        benchmark.strategy = "best_fit";
        benchmark.allocation_mode = "variable_size";
        benchmark.execution_time_ms = 0.375;
        benchmark.peak_memory_usage = 40;
        original.benchmark_results.push_back(benchmark);
        original.final_memory_state_json = "{\"text\":\"quote\\\" and slash\\\\\"}";
        original.errors = {"first error", "second\nline"};

        std::string error;
        assert(store.append(original, error));
        assert(error.empty());
        std::vector<ExperimentRecord> restored;
        assert(store.load(restored, error));
        assert(error.empty());
        assert(restored.size() == 1);
        const auto& record = restored.front();
        assert(record.experiment_id == original.experiment_id);
        assert(record.name == original.name);
        assert(record.timestamp == original.timestamp);
        assert(record.pool_capacity == original.pool_capacity);
        assert(record.strategy == original.strategy);
        assert(record.workload_signature == original.workload_signature);
        assert(record.random_seed == original.random_seed);
        assert(record.operation_sequence.size() == 2);
        assert(record.operation_sequence[0].requested_size == 37);
        assert(record.operation_sequence[1].error_message == "test failure note");
        assert(record.memory_metrics.total_requested_memory == 37);
        assert(record.allocation_metrics.p95_latency_ms == original.allocation_metrics.p95_latency_ms);
        assert(record.fragmentation.internal_fragmentation_bytes == 3);
        assert(record.workload_metrics.allocation_size_distribution == original.workload_metrics.allocation_size_distribution);
        assert(record.benchmark_results.size() == 1);
        assert(record.benchmark_results[0].execution_time_ms == 0.375);
        assert(record.final_memory_state_json == original.final_memory_state_json);
        assert(record.errors == original.errors);
        std::filesystem::remove(storage_path, cleanup_error);
    }

    std::cout << "All allocator tests passed.\n";
    return 0;
}
