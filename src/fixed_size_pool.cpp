#include "fixed_size_pool.h"

FixedSizePool::FixedSizePool(size_t capacity, size_t block_size)
    : pool_(capacity, block_size, MemoryPool::Strategy::FixedSize), block_size_(block_size) {
    initialize();
}

bool FixedSizePool::initialize() {
    return pool_.initialize();
}

void* FixedSizePool::allocate() {
    return pool_.allocate(block_size_, ++allocations_);
}

bool FixedSizePool::deallocate(void* ptr) {
    return pool_.deallocate(ptr);
}

std::vector<AllocationBlock> FixedSizePool::blocks() const {
    return pool_.blocks();
}

MemoryStats FixedSizePool::stats() const {
    return pool_.stats();
}

size_t FixedSizePool::total_blocks() const {
    return pool_.total_capacity() / block_size_;
}
