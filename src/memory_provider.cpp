#include "memory_provider.h"

#include <limits>
#include <new>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include <vector>

namespace {

class HeapMemoryProvider final : public MemoryProvider {
public:
    bool initialize(std::size_t requested_bytes) override {
        if (requested_bytes == 0) {
            return false;
        }
        try {
            storage_.assign(requested_bytes, 0);
        } catch (const std::bad_alloc&) {
            storage_.clear();
            return false;
        }
        return true;
    }

    bool release() override {
        storage_.clear();
        storage_.shrink_to_fit();
        return true;
    }

    std::uint8_t* data() const override {
        return const_cast<std::uint8_t*>(storage_.data());
    }

    std::size_t capacity() const override {
        return storage_.size();
    }

    std::size_t mapped_bytes() const override {
        return storage_.size();
    }

    const char* name() const override {
        return "heap";
    }

private:
    std::vector<std::uint8_t> storage_;
};

class MmapMemoryProvider final : public MemoryProvider {
public:
    ~MmapMemoryProvider() override {
        release();
    }

    bool initialize(std::size_t requested_bytes) override {
        if (requested_bytes == 0 || mapped_address_ != nullptr) {
            return false;
        }
        const long page_size = sysconf(_SC_PAGESIZE);
        if (page_size <= 0) {
            return false;
        }
        const std::size_t page = static_cast<std::size_t>(page_size);
        if (requested_bytes > std::numeric_limits<std::size_t>::max() - (page - 1)) {
            return false;
        }
        const std::size_t rounded_size = ((requested_bytes + page - 1) / page) * page;
        if (rounded_size < requested_bytes) {
            return false;
        }
        void* mapped = mmap(nullptr, rounded_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped == MAP_FAILED) {
            return false;
        }
        mapped_address_ = static_cast<std::uint8_t*>(mapped);
        capacity_ = requested_bytes;
        mapped_bytes_ = rounded_size;
        return true;
    }

    bool release() override {
        if (mapped_address_ == nullptr) {
            return true;
        }
        if (munmap(mapped_address_, mapped_bytes_) != 0) {
            return false;
        }
        mapped_address_ = nullptr;
        capacity_ = 0;
        mapped_bytes_ = 0;
        return true;
    }

    std::uint8_t* data() const override {
        return mapped_address_;
    }

    std::size_t capacity() const override {
        return capacity_;
    }

    std::size_t mapped_bytes() const override {
        return mapped_bytes_;
    }

    const char* name() const override {
        return "mmap";
    }

private:
    std::uint8_t* mapped_address_ = nullptr;
    std::size_t capacity_ = 0;
    std::size_t mapped_bytes_ = 0;
};

}  // namespace

std::unique_ptr<MemoryProvider> make_memory_provider(MemoryProviderType type) {
    if (type == MemoryProviderType::Mmap) {
        return std::make_unique<MmapMemoryProvider>();
    }
    return std::make_unique<HeapMemoryProvider>();
}

const char* memory_provider_type_name(MemoryProviderType type) {
    return type == MemoryProviderType::Mmap ? "mmap" : "heap";
}

bool memory_provider_type_from_name(const char* name, MemoryProviderType& type) {
    if (name == nullptr) {
        return false;
    }
    if (std::string(name) == "heap") {
        type = MemoryProviderType::Heap;
        return true;
    }
    if (std::string(name) == "mmap") {
        type = MemoryProviderType::Mmap;
        return true;
    }
    return false;
}
