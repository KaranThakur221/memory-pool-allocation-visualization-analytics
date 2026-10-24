#pragma once

#include <cstddef>
#include <vector>

#include "memory_pool.h"

class FixedSizePool {
public:
    FixedSizePool(size_t capacity, size_t block_size);
    bool initialize();
    void* allocate();
    bool deallocate(void* ptr);
    std::vector<AllocationBlock> blocks() const;
    MemoryStats stats() const;
    size_t total_blocks() const;

private:
    MemoryPool pool_;
    size_t block_size_ = 64;
    size_t allocations_ = 0;
};
