#pragma once

#include <cstddef>
#include <cstdint>

class MemoryPool;

void* best_fit_allocate(MemoryPool& pool, size_t requested_size, uint64_t allocation_id = 0);
