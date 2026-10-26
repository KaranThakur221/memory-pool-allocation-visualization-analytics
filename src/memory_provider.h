#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

class MemoryProvider {
public:
    virtual ~MemoryProvider() = default;

    virtual bool initialize(std::size_t requested_bytes) = 0;
    virtual bool release() = 0;
    virtual std::uint8_t* data() const = 0;
    virtual std::size_t capacity() const = 0;
    virtual std::size_t mapped_bytes() const = 0;
    virtual const char* name() const = 0;
};

enum class MemoryProviderType {
    Heap,
    Mmap
};

std::unique_ptr<MemoryProvider> make_memory_provider(MemoryProviderType type);
const char* memory_provider_type_name(MemoryProviderType type);
bool memory_provider_type_from_name(const char* name, MemoryProviderType& type);
