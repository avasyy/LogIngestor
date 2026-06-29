#ifndef CLIENT_H
#define CLIENT_H

#include <string>

enum class LogLevel {
    ERROR,
    WARNING,
    INFO
};

class LogClient final {
public:
    LogClient(size_t rate, size_t count, const LogLevel& lvl, const std::string& pipe);
    ~LogClient();

    // Prohibit Copy & Move
    LogClient(const LogClient&) = delete;
    LogClient(LogClient&&) = delete;
    LogClient& operator=(const LogClient&) = delete;
    LogClient& operator=(LogClient&&) = delete;

    void start() const;

private:
    bool writeMsg(size_t seq) const;

private:
    size_t mRate;
    size_t mCount;
    LogLevel mLogLevel;

    std::string mPipePath;
    pid_t mPID;
    int mPipeWFD {-1};
};

#endif  // CLIENT_H