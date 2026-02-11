#ifndef ERRORSTATE_HPP
#define ERRORSTATE_HPP

#include <string>
#include <mutex>
#include "Logger.hpp"  // Assuming you have a Logger class

namespace Marc {

    class ErrorState {
    public:
        // Returns a reference to the singleton instance
        static ErrorState& instance();

        // Sets the last error message
        void set_error(const std::string& message);

        // Retrieves the last error message
        const char* get_last_error();

    private:
        // Private constructor to prevent direct instantiation
        ErrorState() = default;

        // Mutex for thread-safety
        std::mutex error_mutex;

        // Stores the last error message
        std::string last_error;
    };

} // namespace Marc

#endif // ERRORSTATE_HPP
