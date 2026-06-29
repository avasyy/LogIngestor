#include "server.h"

#include <iostream>
#include <filesystem>
#include <future>
#include <csignal>          // sigemptyset, sigaddset, sigprocmask
#include <cstring>          // std::strerror
#include <fcntl.h>          // Required for O_RDONLY, O_WRONLY, O_NONBLOCK
#include <sys/stat.h>       // Required for mkfifo
#include <cstdio>           // Required for remove
#include <unistd.h>         // Required for read()
#include <sys/epoll.h>      // Required for epoll()
#include <sys/signalfd.h>   // signalfd

LogServer::LogServer(const std::string& pipe, const std::string& log)
    : mPipePath(pipe), mLogFileName(log), mLogFile(log)
{

}

LogServer::~LogServer()
{
    if (mSignalFD > 0) close(mSignalFD);
    if (mEpollFD > 0) close(mEpollFD);
    if (mPipeRFD > 0) close(mPipeRFD);
    if (mPipeWFD > 0) close(mPipeWFD);

    // EPOLL CLOSING LATER

    // Remove pipe
    closePIPE();
}

void LogServer::start()
{
    createPIPE();

    if (mPipeRFD < 0)
    {
        initPIPE();
    }
    else {
        std::cerr << "The pipe descriptor is already opened" << std::endl;
        return;
    }

    // ----- epoll instance -----
    mEpollFD = epoll_create1(0);
    if (mEpollFD < 0)
    {
        throw std::runtime_error(std::string("epoll_create1: ") + std::strerror(errno));
    }

    // ----- Register the PIPE read end -----
    struct epoll_event ev{};
    ev.events  = EPOLLIN; // start LT; add | EPOLLET later to switch
    ev.data.fd = mPipeRFD;
    epoll_ctl(mEpollFD, EPOLL_CTL_ADD, mPipeRFD, &ev);

    // ----- siglalfd: turn SIGINT/SIGTERM into a readable fd -----
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // block default handling, or the kernel still terminates us
    if (sigprocmask(SIG_BLOCK, &mask, nullptr) < 0)
    {
        throw std::runtime_error(std::string("sigprocmask: ") + std::strerror(errno));
    }

    mSignalFD = signalfd(-1, &mask, 0);
    if (mSignalFD < 0)
    {
        throw std::runtime_error(std::string("signalfd: ") + std::strerror(errno));
    }

    // register the signal fd alongside the pipe
    struct epoll_event sev{};
    sev.events  = EPOLLIN;
    sev.data.fd = mSignalFD;
    if (epoll_ctl(mEpollFD, EPOLL_CTL_ADD, mSignalFD, &sev) < 0)
    {
        throw std::runtime_error(std::string("epoll_ctl(signal): ") + std::strerror(errno));
    }

    // ----- event loop -----
    MessageFramer framer;
    char buf[4096];
    struct epoll_event events[64];
    bool shutting_down = false;

    while (!shutting_down)
    {
        int nready = epoll_wait(mEpollFD, events, 64, -1);   // sleeps at 0 CPU until ready
        if (nready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nready; i++)
        {
            // Defensive: handle hangup/error before assuming readable
            if (events[i].events & (EPOLLHUP | EPOLLERR))
            {
                std::cerr << "unexpected HUP/ERR on fd\n";
                continue;
            }
            if (!(events[i].events & EPOLLIN))
            {
                continue;
            }

            // ----- shutdown signal -----
            if (events[i].data.fd == mSignalFD)
            {
                struct signalfd_siginfo si{};
                ssize_t r = read(mSignalFD, &si, sizeof(si)); // consume the event
                if (r == sizeof(si))
                {
                    std::cout << "Received signal " << si.ssi_signo << ", shutting down\n";
                }
                shutting_down = true;
                break;  // leave the for-loop; while-condition ends the outer loop
            }

            // ----- log data -----
            if (events[i].data.fd == mPipeRFD)
            {
                // Drain until EAGAIN
                for (;;)
                {
                    ssize_t n = read(mPipeRFD, buf, sizeof(buf));
                    if (n > 0)
                    {
                        for (auto&& msg: framer.feed(buf, n))
                        {
                            writeMsgToLog(msg);
                        }
                    }
                    else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
                    {
                        break; // buffer empty -> back to epoll_wait
                    }
                    else if (n < 0 && errno == EINTR)
                    {
                        continue; // retry
                    }
                    else
                    {
                        break; // n==0 (can't happen w/ wfd) or error
                    }
                }
            }
        }
    }

    // ----- graceful shutdown (task: close FIFO + flush log) -----
    mLogFile.flush();
    std::cout << "Log flushed, exiting cleanly\n";
}

void LogServer::initPIPE()
{
    /*
     * Single PIPE (FIFO) is not enough in this case
     * as reading from it with getline() will work only
     * once for first message from the very first client.
     * All further writes won't be seen on pipe
     * as it will be blocked on EOF.
     * 
     * To eliminate mentioned problem the PIPE should be opened
     * with fd (file descriptor) to access it directly with
     * system calls like read()/write().
     * 
     * rfd - it is fd for reading only purposes
     * and it's opened as O_NONBLOCK.
     * 
     * There are two options for opening fd BLOCKING & !BLOCKING
     * The whole difference depends on whether the data is on PIPE or not.
     * - In case PIPE has some bytes - the read() call will copy data to buffer
     * and return how many bytes were copied.
     * - In case PIPE has no bytes - there is difference for BLOCKING & !BLOCKING
     *   1) BLOCKING - the kernel will put the thread to sleep until data arrives
     *   2) !BLOCKING - the kernel does not sleep. It immediately returns -1 and
     *      sets errno = EAGAIN (nothing right now - try later, your move) 
     */
    mPipeRFD = open(mPipePath.c_str(), O_RDONLY | O_NONBLOCK);
    /*
     * There should be at least one active writer
     * to eliminate EOF, which is returned when no active writers left.
     * 
     * With persistent active dummy writer we guarantee to get EAGAIN
     * on read() instead of EOF. The EOF is dangerous with epoll as
     * we will reenter the epoll_wait loop and immediately be told the fd is
     * readable again — EOF is a sticky, level-triggered condition, so each
     * read() returns 0 instantly and we spin at 100% CPU forever.
    */
    mPipeWFD = open(mPipePath.c_str(), O_WRONLY);

    // Exit in case open() failed
    if (mPipeRFD < 0 || mPipeWFD < 0)
    {
        if (mPipeRFD >= 0) close(mPipeRFD);
        if (mPipeWFD >= 0) close(mPipeWFD);
        throw std::runtime_error("failed to open FIFO " + mPipePath + ": " + std::strerror(errno));
    }
}

void LogServer::createPIPE()
{
    if (mkfifo(mPipePath.c_str(), 0666) == 0)
    {
        std::cout << "Created FIFO: " << mPipePath << "\n";
        return;
    }
    if (errno == EEXIST)
    {
        std::cout << "FIFO already exists, reusing: " << mPipePath << "\n";
        return;
    }
    // any other errno is a genuine failure
    throw std::runtime_error("mkfifo failed for " + mPipePath + ": " + std::strerror(errno));
}

void LogServer::closePIPE()
{
    if (std::remove(mPipePath.c_str()) == -1)
    {
        std::cerr << "Failed to remove named pipe: " << mPipePath << std::endl;
    }
    else
    {
        std::cout << "The pipe: " << mPipePath << " was successfully removed!" << std::endl;
    }
}

void LogServer::writeMsgToLog(const std::string& msg)
{
    if (checkLogSize() > mRotateSize)
    {
        logRotate();
        std::cout << "Log rotation succeeded!" << mLogFileName << std::endl;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
    localtime_r(&t, &tm); // thread-safe localtime (POSIX)

    char ts[32];
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm); // "2026-06-27 14:03:01"

    mLogFile << ts << " " << msg << std::endl;
}

std::uintmax_t LogServer::checkLogSize() const
{
    std::error_code ec;
    auto size = std::filesystem::file_size(mLogFileName, ec);
    if (ec) return 0;
    return size;
}

/*
 * Log rotation process
 *
 * The logs are being written to single file with name <app>.log
 * in case the size of <app>.log exceeds the 1MB, the log rotation kicks in.
 * 
 * The main idea of log rotation is to keep writing logs to <app>.log
 * when the previous snapshot with size of 1MB is saved to <app[N]>.log
*/
void LogServer::logRotate()
{
    auto rotatedName = [&](std::size_t n) -> std::string {
        return mLogFileName + "." + std::to_string(n);
    };

    mLogFile.close();

    // Remove the oldest file (if it exist)
    std::remove(rotatedName(mMaxLogFiles).c_str());

    // Shifting log file names
    // 
    for (std::size_t i = mMaxLogFiles - 1; i > 0; --i)
    {
        std::rename(rotatedName(i).c_str(), rotatedName(i + 1).c_str());
    }

    // active to first
    std::rename(mLogFileName.c_str(), rotatedName(1).c_str());

    mLogFile.clear();
    mLogFile.open(mLogFileName, std::ios::trunc);
}