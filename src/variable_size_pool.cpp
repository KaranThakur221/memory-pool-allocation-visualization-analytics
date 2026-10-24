#include "variable_size_pool.h"

VariableSizePool::VariableSizePool(size_t capacity, MemoryPool::Strategy strategy)
    : pool_(capacity, 64, strategy) {}

bool VariableSizePool::initialize() {
    return pool_.initialize();
}

void* VariableSizePool::allocate(size_t requested_size, uint64_t allocation_id) {
    return pool_.allocate(requested_size, allocation_id);
}

bool VariableSizePool::deallocate(void* ptr) {
    return pool_.deallocate(ptr);
}

std::vector<AllocationBlock> VariableSizePool::blocks() const {
    return pool_.blocks();
}

MemoryStats VariableSizePool::stats() const {
    return pool_.stats();
}

MemoryPool::Strategy VariableSizePool::strategy() const {
    return pool_.strategy();
}
