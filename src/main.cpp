#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "benchmark.h"
#include "fixed_size_pool.h"
#include "memory_pool.h"
#include "variable_size_pool.h"

int main() {
    std::cout << "Custom Memory Pool Allocator Demo\n";

    MemoryPool pool(256 * 1024, 64, MemoryPool::Strategy::FirstFit);
    auto* p1 = static_cast<int*>(pool.allocate(64, 10));
    auto* p2 = static_cast<int*>(pool.allocate(128, 11));
    if (p1 != nullptr) {
        std::cout << "Allocated p1\n";
    }
    if (p2 != nullptr) {
        std::cout << "Allocated p2\n";
    }
    pool.deallocate(p1);
    std::cout << "Pool JSON:\n" << pool.to_json() << "\n";

    FixedSizePool fixed_pool(1 * 1024 * 1024, 64);
    std::vector<void*> fixed_allocations;
    for (int i = 0; i < 32; ++i) {
        auto* ptr = fixed_pool.allocate();
        if (ptr != nullptr) {
            fixed_allocations.push_back(ptr);
        }
    }
    for (void* ptr : fixed_allocations) {
        fixed_pool.deallocate(ptr);
    }

    VariableSizePool variable_pool(512 * 1024, MemoryPool::Strategy::BestFit);
    std::vector<size_t> sizes = {32, 64, 128, 256, 512, 1024};
    std::vector<void*> variable_allocations;
    for (size_t size : sizes) {
        auto* ptr = variable_pool.allocate(size, 42);
        if (ptr != nullptr) {
            variable_allocations.push_back(ptr);
        }
    }
    for (void* ptr : variable_allocations) {
        variable_pool.deallocate(ptr);
    }

    std::vector<size_t> workload = {32, 64, 128, 256, 512, 64, 128, 256};
    auto benchmark = benchmark_pool(1 * 1024 * 1024, workload, MemoryPool::Strategy::BestFit, 2);
    std::cout << "Benchmark allocation_latency_ns=" << benchmark.allocation_latency_ns << "\n";

    std::filesystem::create_directories("frontend/public/data");
    std::ofstream json_output("frontend/public/data/pool_state.json");
    json_output << pool.to_json();

    std::cout << "Demo completed successfully.\n";
    return 0;
}
