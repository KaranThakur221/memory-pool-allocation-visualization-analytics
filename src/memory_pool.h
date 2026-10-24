#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct AllocationBlock {
    uint64_t block_id = 0;
    size_t offset = 0;
    size_t size = 0;
    size_t requested_size = 0;
    bool allocated = false;
    uint64_t allocation_id = 0;
    std::string state = "free";
};

struct MemoryStats {
    size_t total_capacity = 0;
    size_t allocated_memory = 0;
    size_t free_memory = 0;
    size_t requested_memory = 0;
    size_t active_allocations = 0;
    size_t successful_allocations = 0;
    size_t failed_allocations = 0;
    size_t deallocations = 0;
    size_t free_block_count = 0;
    size_t largest_free_block = 0;
    size_t peak_memory_usage = 0;
    double memory_utilization = 0.0;
    double average_memory_utilization = 0.0;
    double peak_memory_utilization = 0.0;
    double internal_fragmentation = 0.0;
    double external_fragmentation = 0.0;
    double allocation_success_rate = 0.0;
    double allocation_failure_rate = 0.0;
    double internal_fragmentation_percent = 0.0;
    double average_free_block_size = 0.0;
    size_t contiguous_memory_failures = 0;
    size_t successful_coalescing_operations = 0;
    size_t block_splitting_operations = 0;
};

class MemoryPool {
public:
    enum class Strategy {
        FirstFit,
        BestFit,
        FixedSize,
        VariableSize
    };

    MemoryPool(size_t capacity = 1024 * 1024, size_t block_size = 64, Strategy strategy = Strategy::FirstFit);
    ~MemoryPool() = default;

    bool initialize();
    bool is_initialized() const;
    void* allocate(size_t requested_size, uint64_t allocation_id = 0);
    bool deallocate(void* ptr);
    void reset();

    std::vector<AllocationBlock> blocks() const;
    std::vector<AllocationBlock> free_blocks() const;
    MemoryStats stats() const;
    bool can_allocate(size_t requested_size) const;
    size_t total_capacity() const;
    size_t total_free_memory() const;
    size_t largest_free_block() const;
    size_t active_allocations() const;
    std::string to_json() const;

    Strategy strategy() const;
    size_t block_size() const;

private:
    AllocationBlock* find_candidate(size_t requested_size);
    AllocationBlock* find_first_fit(size_t requested_size);
    AllocationBlock* find_best_fit(size_t requested_size);
    void merge_adjacent_free_blocks();
    void refresh_free_list();

    std::vector<uint8_t> buffer_;
    std::vector<AllocationBlock> blocks_;
    std::vector<AllocationBlock> free_blocks_;
    size_t capacity_ = 0;
    size_t block_size_ = 64;
    Strategy strategy_ = Strategy::FirstFit;
    uint64_t next_allocation_id_ = 1;
    uint64_t next_block_id_ = 1;
    size_t successful_allocations_ = 0;
    size_t failed_allocations_ = 0;
    size_t deallocations_ = 0;
    size_t peak_memory_usage_ = 0;
    size_t coalescing_operations_ = 0;
    size_t block_splitting_operations_ = 0;
    size_t contiguous_memory_failures_ = 0;
    bool initialized_ = false;
    static constexpr size_t kMinAllocationSize = 16;
};
