#include "best_fit.h"

#include "memory_pool.h"

void* best_fit_allocate(MemoryPool& pool, size_t requested_size, uint64_t allocation_id) {
    return pool.allocate(requested_size, allocation_id);
}
