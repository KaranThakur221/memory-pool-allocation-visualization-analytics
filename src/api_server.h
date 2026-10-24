#pragma once

#include <cstddef>
#include <cstdint>

class PoolApiServer {
public:
    PoolApiServer(size_t pool_capacity = 256 * 1024, unsigned short port = 8080);
    ~PoolApiServer();

    int run();

private:
    size_t pool_capacity_;
    unsigned short port_;
    uint64_t next_allocation_id_ = 1;
    bool running_ = false;
};
