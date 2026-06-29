#include "client.h"

#include <map>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <fcntl.h>  // Required for O_WRONLY

const static std::map<LogLevel, std::string> logLevels = {
    {LogLevel::INFO, "INFO"},
    {LogLevel::WARNING, "WARNING"},
    {LogLevel::ERROR, "ERROR"}
};

LogClient::LogClient(size_t rate, size_t count, const LogLevel& lvl, const std::string& pipe)
    : mRate(rate), mCount(count), mLogLevel(lvl), mPipePath(pipe)
{
    mPID = getpid();
    mPipeWFD = open(pipe.c_str(), O_WRONLY);

    if (mPipeWFD < 0) {
        std::cerr << "Failed to open FIFO " << pipe << " for writing: "
            << std::strerror(errno) << std::endl;
    } else {
    }
}

LogClient::~LogClient()
{
    if (mPipeWFD >= 0) close(mPipeWFD);
}

void LogClient::start() const
{
    if (mPipeWFD < 0 || !mRate) return;

    auto start = std::chrono::steady_clock::now();
    auto interval = std::chrono::microseconds(1'000'000 / mRate);

    for (size_t seq=0; seq < mCount; ++seq) {
        writeMsg(seq);

        // the deadline for the NEXT message, measured from the fixed start
        auto next = start + interval * (seq + 1);
        std::this_thread::sleep_until(next); // sleep_until, not sleep_for
    }
}

bool LogClient::writeMsg(size_t seq) const
{
    std::string line = "LEVEL=" + logLevels.at(mLogLevel) +
        " pid=" + std::to_string(mPID) + " msg=\"hello\" seq=" + std::to_string(seq) + "\n";
    ssize_t written = write(mPipeWFD, line.c_str(), line.size());

    if (written < 0)
    {
        if (errno == EPIPE) {
            std::cerr << "reader closed the FIFO (EPIPE); stopping\n";
            return false;
        }
        
        std::cerr << "write failed: " << std::strerror(errno) << "\n";
        return false;
    }
    if (static_cast<size_t>(written) != line.size())
    {
        std::cerr << "short write: " << written << "/" << line.size() << " bytes\n";
        return false;
    }

    return true;
}