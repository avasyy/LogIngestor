#include <iostream>

#include "server.h"

constexpr const char* PIPE = "--pipe";
constexpr const char* LOG  = "--log";

int main(int argc, char* argv[]) {
    std::string pipe = "/tmp/fifo";
    std::string log = "/tmp/app.log";

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << name << " requires a value" << std::endl;
                std::exit(1);
            }

            return argv[++i];
        };

        if (arg == PIPE)
        {
            pipe = next(PIPE);
        }
        else if (arg == LOG)
        {
            log = next(LOG);
        }
        else
        {
            std::cerr << "unknown argument: " << arg << "\n";
            return 1;
        }
    }

    try {
        LogServer server(pipe, log);
        server.start();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
