#include "messageframer.h"

std::vector<std::string> MessageFramer::feed(const char* data, size_t len) {
    acc.append(data, len);
    std::vector<std::string> messages;
    size_t pos;
    while ((pos = acc.find('\n')) != std::string::npos) {
        messages.push_back(acc.substr(0, pos));
        acc.erase(0, pos + 1);
    }
    return messages;
}