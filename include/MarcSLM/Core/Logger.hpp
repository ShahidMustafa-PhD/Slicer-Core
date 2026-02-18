// ==============================================================================
// MarcSLM - Logger (Thread-safe Async Logging)
// ==============================================================================
// Ported from Legacy Logger.hpp
// Provides singleton thread-safe logging with async worker thread
// ==============================================================================

#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <fstream>
#include <sstream>
#include <atomic>
#include <condition_variable>
#include <thread>

namespace MarcSLM {

/// @brief Thread-safe asynchronous logger.
/// @details Singleton that queues log messages and writes them in a
///          background thread. Safe to call from any thread.
///          Ported from Legacy Marc::Logger.
class Logger {
public:
    /// @brief Get the singleton instance.
    static Logger& instance();

    /// @brief Queue a log message.
    void log(const std::string& message);

    /// @brief Start the background worker thread.
    void start();

    /// @brief Stop the background worker and flush remaining messages.
    void stop();

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void worker();

    std::ofstream               log_file;
    std::queue<std::string>     log_queue;
    std::mutex                  queue_mutex;
    std::condition_variable     cv;
    std::atomic<bool>           running;
    std::thread                 worker_thread;
};

} // namespace MarcSLM
