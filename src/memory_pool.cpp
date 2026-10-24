#include "memory_pool.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

MemoryPool::MemoryPool(size_t capacity, size_t block_size, Strategy strategy)
    : capacity_(capacity), block_size_(block_size), strategy_(strategy) {
    initialize();
}

bool MemoryPool::initialize() {
    if (capacity_ == 0 || (strategy_ == Strategy::FixedSize && block_size_ == 0)) {
        return false;
    }

    successful_allocations_ = 0;
    failed_allocations_ = 0;
    deallocations_ = 0;
    peak_memory_usage_ = 0;
    coalescing_operations_ = 0;
    block_splitting_operations_ = 0;
    contiguous_memory_failures_ = 0;
    next_allocation_id_ = 1;
    next_block_id_ = 1;

    buffer_.assign(capacity_, 0);
    blocks_.clear();
    free_blocks_.clear();

    blocks_.push_back({next_block_id_++, 0, capacity_, 0, false, 0, "free"});
    free_blocks_.push_back(blocks_.front());
    initialized_ = true;
    return true;
}

bool MemoryPool::is_initialized() const {
    return initialized_;
}

void MemoryPool::reset() {
    initialized_ = false;
    successful_allocations_ = 0;
    failed_allocations_ = 0;
    deallocations_ = 0;
    peak_memory_usage_ = 0;
    coalescing_operations_ = 0;
    block_splitting_operations_ = 0;
    contiguous_memory_failures_ = 0;
    buffer_.assign(capacity_, 0);
    blocks_.clear();
    free_blocks_.clear();
    next_allocation_id_ = 1;
    next_block_id_ = 1;
    blocks_.push_back({next_block_id_++, 0, capacity_, 0, false, 0, "free"});
    free_blocks_.push_back(blocks_.front());
    initialized_ = true;
}

AllocationBlock* MemoryPool::find_first_fit(size_t requested_size) {
    for (auto& block : blocks_) {
        if (!block.allocated && block.size >= requested_size) {
            return &block;
        }
    }
    return nullptr;
}

AllocationBlock* MemoryPool::find_best_fit(size_t requested_size) {
    AllocationBlock* best = nullptr;
    for (auto& block : blocks_) {
        if (block.allocated || block.size < requested_size) {
            continue;
        }
        if (best == nullptr || block.size < best->size) {
            best = &block;
        }
    }
    return best;
}

AllocationBlock* MemoryPool::find_candidate(size_t requested_size) {
    if (strategy_ == Strategy::BestFit) {
        return find_best_fit(requested_size);
    }
    return find_first_fit(requested_size);
}

void MemoryPool::merge_adjacent_free_blocks() {
    std::sort(blocks_.begin(), blocks_.end(), [](const AllocationBlock& a, const AllocationBlock& b) {
        return a.offset < b.offset;
    });

    std::vector<AllocationBlock> merged;
    merged.reserve(blocks_.size());
    size_t coalesced_blocks = 0;
    for (const auto& current : blocks_) {
        if (merged.empty()) {
            merged.push_back(current);
            continue;
        }
        auto& previous = merged.back();
        if (!previous.allocated && !current.allocated && previous.offset + previous.size == current.offset) {
            previous.size += current.size;
            ++coalesced_blocks;
        } else {
            merged.push_back(current);
        }
    }

    blocks_ = std::move(merged);
    for (auto& block : blocks_) {
        block.state = block.allocated ? "allocated" : "free";
    }
    coalescing_operations_ += coalesced_blocks;
    refresh_free_list();
}

void MemoryPool::refresh_free_list() {
    free_blocks_.clear();
    for (auto& block : blocks_) {
        if (!block.allocated) {
            free_blocks_.push_back(block);
        }
    }
}

void* MemoryPool::allocate(size_t requested_size, uint64_t allocation_id) {
    if (!initialized_ || requested_size == 0 || requested_size > capacity_) {
        ++failed_allocations_;
        ++contiguous_memory_failures_;
        return nullptr;
    }

    const size_t original_requested_size = requested_size;
    if (strategy_ == Strategy::FixedSize && requested_size > block_size_) {
        ++failed_allocations_;
        ++contiguous_memory_failures_;
        return nullptr;
    }

    size_t aligned_size = strategy_ == Strategy::FixedSize ? block_size_ : requested_size;
    if (aligned_size % 8 != 0) {
        if (aligned_size > std::numeric_limits<size_t>::max() - 7) {
            ++failed_allocations_;
            ++contiguous_memory_failures_;
            return nullptr;
        }
        aligned_size = ((aligned_size / 8) + 1) * 8;
    }

    AllocationBlock* candidate = find_candidate(aligned_size);
    if (candidate == nullptr) {
        ++failed_allocations_;
        ++contiguous_memory_failures_;
        return nullptr;
    }

    const size_t original_offset = candidate->offset;
    const size_t original_size = candidate->size;
    candidate->allocated = true;
    candidate->requested_size = original_requested_size;
    candidate->state = "allocated";
    candidate->allocation_id = allocation_id == 0 ? next_allocation_id_++ : allocation_id;
    ++successful_allocations_;

    if (original_size > aligned_size && (original_size - aligned_size) >= kMinAllocationSize) {
        AllocationBlock remainder = {next_block_id_++, original_offset + aligned_size, original_size - aligned_size, 0, false, 0, "free"};
        candidate->size = aligned_size;
        auto it = std::find_if(blocks_.begin(), blocks_.end(), [&](const AllocationBlock& block) {
            return block.offset == original_offset && block.block_id == candidate->block_id;
        });
        if (it == blocks_.end()) {
            --successful_allocations_;
            ++failed_allocations_;
            ++contiguous_memory_failures_;
            return nullptr;
        }
        ++it;
        blocks_.insert(it, remainder);
        ++block_splitting_operations_;
    }

    refresh_free_list();
    size_t allocated_total = 0;
    for (const auto& block : blocks_) {
        if (block.allocated) allocated_total += block.size;
    }
    peak_memory_usage_ = std::max(peak_memory_usage_, allocated_total);
    return buffer_.data() + original_offset;
}

bool MemoryPool::deallocate(void* ptr) {
    if (ptr == nullptr) {
        return false;
    }

    const auto start = reinterpret_cast<std::uintptr_t>(buffer_.data());
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    if (address < start || address - start >= buffer_.size()) {
        return false;
    }

    const size_t offset = static_cast<size_t>(address - start);
    auto it = std::find_if(blocks_.begin(), blocks_.end(), [offset](const AllocationBlock& block) {
        return block.allocated && block.offset == offset;
    });
    if (it == blocks_.end()) {
        return false;
    }

    it->allocated = false;
    it->requested_size = 0;
    it->allocation_id = 0;
    it->state = "free";
    ++deallocations_;

    merge_adjacent_free_blocks();
    return true;
}

std::vector<AllocationBlock> MemoryPool::blocks() const {
    return blocks_;
}

std::vector<AllocationBlock> MemoryPool::free_blocks() const {
    return free_blocks_;
}

MemoryStats MemoryPool::stats() const {
    MemoryStats stats;
    stats.total_capacity = capacity_;
    stats.free_block_count = 0;
    stats.largest_free_block = 0;
    stats.successful_allocations = successful_allocations_;
    stats.failed_allocations = failed_allocations_;
    stats.deallocations = deallocations_;
    stats.contiguous_memory_failures = contiguous_memory_failures_;
    stats.successful_coalescing_operations = coalescing_operations_;
    stats.block_splitting_operations = block_splitting_operations_;
    size_t allocated_total = 0;
    size_t requested_total = 0;
    size_t free_total = 0;

    for (const auto& block : blocks_) {
        if (block.allocated) {
            allocated_total += block.size;
            requested_total += block.requested_size;
            stats.active_allocations += 1;
        } else {
            free_total += block.size;
            stats.free_block_count += 1;
            stats.largest_free_block = std::max(stats.largest_free_block, block.size);
        }
    }

    stats.allocated_memory = allocated_total;
    stats.free_memory = free_total;
    stats.requested_memory = requested_total;
    stats.peak_memory_usage = peak_memory_usage_;
    stats.memory_utilization = capacity_ == 0 ? 0.0 : (static_cast<double>(allocated_total) / static_cast<double>(capacity_));
    stats.peak_memory_utilization = capacity_ == 0 ? 0.0 : (static_cast<double>(peak_memory_usage_) / static_cast<double>(capacity_));
    stats.average_memory_utilization = stats.memory_utilization;
    stats.internal_fragmentation = allocated_total >= requested_total ? static_cast<double>(allocated_total - requested_total) : 0.0;
    stats.internal_fragmentation_percent = requested_total == 0 ? 0.0 : (100.0 * stats.internal_fragmentation / static_cast<double>(requested_total));
    if (stats.free_memory == 0) {
        stats.external_fragmentation = 0.0;
    } else {
        stats.external_fragmentation = std::max(0.0, 1.0 - (static_cast<double>(stats.largest_free_block) / static_cast<double>(stats.free_memory)));
    }
    stats.average_free_block_size = stats.free_block_count == 0 ? 0.0 : static_cast<double>(stats.free_memory) / static_cast<double>(stats.free_block_count);
    stats.allocation_success_rate = (stats.successful_allocations + stats.failed_allocations) == 0
        ? 0.0
        : static_cast<double>(stats.successful_allocations) / static_cast<double>(stats.successful_allocations + stats.failed_allocations);
    stats.allocation_failure_rate = 1.0 - stats.allocation_success_rate;
    return stats;
}

bool MemoryPool::can_allocate(size_t requested_size) const {
    if (requested_size == 0 || requested_size > capacity_) {
        return false;
    }
    for (const auto& block : blocks_) {
        if (!block.allocated && block.size >= requested_size) {
            return true;
        }
    }
    return false;
}

size_t MemoryPool::total_capacity() const {
    return capacity_;
}

size_t MemoryPool::total_free_memory() const {
    size_t total = 0;
    for (const auto& block : blocks_) {
        if (!block.allocated) {
            total += block.size;
        }
    }
    return total;
}

size_t MemoryPool::largest_free_block() const {
    size_t largest = 0;
    for (const auto& block : blocks_) {
        if (!block.allocated) {
            largest = std::max(largest, block.size);
        }
    }
    return largest;
}

size_t MemoryPool::active_allocations() const {
    size_t count = 0;
    for (const auto& block : blocks_) {
        if (block.allocated) {
            ++count;
        }
    }
    return count;
}

MemoryPool::Strategy MemoryPool::strategy() const {
    return strategy_;
}

size_t MemoryPool::block_size() const {
    return block_size_;
}

std::string MemoryPool::to_json() const {
    std::ostringstream out;
    out << "{\n";
    out << "  \"capacity\": " << capacity_ << ",\n";
    out << "  \"block_size\": " << block_size_ << ",\n";
    out << "  \"pool_strategy\": \"";
    switch (strategy_) {
        case Strategy::FirstFit: out << "first_fit"; break;
        case Strategy::BestFit: out << "best_fit"; break;
        case Strategy::FixedSize: out << "fixed_size"; break;
        case Strategy::VariableSize: out << "variable_size"; break;
    }
    out << "\",\n";
    out << "  \"blocks\": [\n";
    for (size_t i = 0; i < blocks_.size(); ++i) {
        const auto& block = blocks_[i];
        out << "    {\"id\": " << block.block_id
            << ", \"offset\": " << block.offset
            << ", \"size\": " << block.size
            << ", \"requested_size\": " << block.requested_size
            << ", \"allocated\": " << (block.allocated ? "true" : "false")
            << ", \"allocation_id\": " << block.allocation_id
            << ", \"state\": \"" << block.state << "\"}";
        if (i + 1 < blocks_.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";
    const MemoryStats current_stats = stats();
    out << "  \"statistics\": {\n";
    out << "    \"allocated_memory\": " << current_stats.allocated_memory << ",\n";
    out << "    \"free_memory\": " << current_stats.free_memory << ",\n";
    out << "    \"requested_memory\": " << current_stats.requested_memory << ",\n";
    out << "    \"largest_free_block\": " << current_stats.largest_free_block << ",\n";
    out << "    \"free_block_count\": " << current_stats.free_block_count << ",\n";
    out << "    \"memory_utilization\": " << current_stats.memory_utilization << ",\n";
    out << "    \"peak_memory_usage\": " << current_stats.peak_memory_usage << ",\n";
    out << "    \"internal_fragmentation\": " << current_stats.internal_fragmentation << ",\n";
    out << "    \"external_fragmentation\": " << current_stats.external_fragmentation << "\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}
