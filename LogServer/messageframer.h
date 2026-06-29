#ifndef MESSAGEFRAMER_H
#define MESSAGEFRAMER_H

#include <string>
#include <vector>

class MessageFramer final {
public:
    MessageFramer() = default;

    // Prohibit Copy & Move
    MessageFramer(const MessageFramer&) = delete;
    MessageFramer(MessageFramer&&) = delete;
    MessageFramer& operator=(const MessageFramer&) = delete;
    MessageFramer& operator=(MessageFramer&&) = delete;

    std::vector<std::string> feed(const char* data, size_t len);

private:
    std::string acc;
};

#endif  // MESSAGEFRAMER_H