#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <fstream>
#include <cstdint>

#include "messageframer.h"

class LogServer final {
public:
    LogServer(const std::string& pipe, const std::string& log);
    ~LogServer();

    // Prohibit Copy & Move
    LogServer(const LogServer&) = delete;
    LogServer(LogServer&&) = delete;
    LogServer& operator=(const LogServer&) = delete;
    LogServer& operator=(LogServer&&) = delete;

    void start();

private:
    void initPIPE();

    void createPIPE();
    void closePIPE();

    void writeMsgToLog(const std::string& msg);

    std::uintmax_t checkLogSize() const;
    void logRotate();

private:
    std::string mPipePath;
    std::string mLogFileName;
    std::ofstream mLogFile;

    const std::size_t mMaxLogFiles {5};
    const std::uintmax_t mRotateSize = 1024 * 1024; // ONE MB

    int mPipeRFD {-1};
    int mPipeWFD {-1}; // Keepalive
    int mEpollFD {-1};
    int mSignalFD {-1};
};

#endif  // SERVER_H
