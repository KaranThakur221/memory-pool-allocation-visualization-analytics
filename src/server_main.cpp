#include "api_server.h"

#include <cstdlib>

int main(int argc, char* argv[]) {
    unsigned short port = 8080;
    if (argc > 1) {
        char* end = nullptr;
        const unsigned long parsed_port = std::strtoul(argv[1], &end, 10);
        if (end == argv[1] || *end != '\0' || parsed_port == 0 || parsed_port > 65535) {
            return 2;
        }
        port = static_cast<unsigned short>(parsed_port);
    }

    PoolApiServer server(256 * 1024, port);
    return server.run();
}
