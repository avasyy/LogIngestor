#include <string>
#include <cstdlib>
#include <iostream>

#include "client.h"

constexpr const char* RATE = "--rate";
constexpr const char* COUNT = "--count";
constexpr const char* LEVEL = "--level";
constexpr const char* PIPE = "--pipe";

int main(int argc, char* argv[]) {
    size_t rate = 50;
    size_t count = 1000;
    LogLevel level = LogLevel::INFO;
    std::string pipe = "/tmp/fifo";

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

        if (arg == RATE)
        {
            rate = std::stoul(next(RATE));
        }
        else if (arg == COUNT)
        {
            count = std::stoul(next(COUNT));
        }
        else if (arg == LEVEL)
        {
            std::string lvl = next(LEVEL);

            if (arg == "INFO") level = LogLevel::INFO;
            else if (arg == "WARNING") level = LogLevel::WARNING;
            else if (arg == "ERROR") level = LogLevel::ERROR;
            else {
                std::cerr << "bad --level: " << lvl << std::endl;
                std::exit(1); 
            }
        }
        else if (arg == PIPE)
        {
            pipe = next(PIPE);
        }
        else
        {
            std::cerr << "unknown argument: " << arg << "\n";
            std::exit(1);
        }
    }

    LogClient client(rate, count, level, pipe);
    client.start();

    return 0;
}
