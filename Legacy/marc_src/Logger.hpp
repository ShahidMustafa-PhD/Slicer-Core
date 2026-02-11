// Logger.hpp
#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <fstream>
#include <sstream>
#include <atomic>
#include <condition_variable>
#include <thread>

namespace Marc {

class Logger {
public:
    static Logger& instance();

    void log(const std::string& message);
    void start();
    void stop();

private:
    Logger();
    ~Logger();

    void worker();

    std::ofstream log_file;
    std::queue<std::string> log_queue;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::atomic<bool> running;
    std::thread worker_thread;
};

} // namespace Marc

