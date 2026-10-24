#pragma once

#include <cstddef>
#include <vector>

#include "memory_pool.h"

class VariableSizePool {
public:
    VariableSizePool(size_t capacity, MemoryPool::Strategy strategy = MemoryPool::Strategy::FirstFit);
    bool initialize();
    void* allocate(size_t requested_size, uint64_t allocation_id = 0);
    bool deallocate(void* ptr);
    std::vector<AllocationBlock> blocks() const;
    MemoryStats stats() const;
    MemoryPool::Strategy strategy() const;

private:
    MemoryPool pool_;
};
