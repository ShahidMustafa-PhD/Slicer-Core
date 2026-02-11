// Logger.cpp
#include "Logger.hpp"
#include <iostream>
#include <chrono>
#include <ctime>

namespace Marc {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : running(false) {
    log_file.open("marc_log.txt", std::ios::app);
}

Logger::~Logger() {
    stop();
    if (log_file.is_open()) log_file.close();
}

void Logger::start() {
    running = true;
    worker_thread = std::thread(&Logger::worker, this);
}

void Logger::stop() {
    running = false;
    cv.notify_all();
    if (worker_thread.joinable()) worker_thread.join();
}

void Logger::log(const std::string& message) {
    std::lock_guard<std::mutex> lock(queue_mutex);
    std::ostringstream oss;
    auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    oss << std::ctime(&now) << ": " << message << "\n";
    log_queue.push(oss.str());
    cv.notify_one();
}

void Logger::worker() {
    while (running) {
        std::unique_lock<std::mutex> lock(queue_mutex);
        cv.wait(lock, [this]() { return !log_queue.empty() || !running; });

        while (!log_queue.empty()) {
            log_file << log_queue.front();
            log_queue.pop();
        }
        log_file.flush();
    }
}

} // namespace Marc
