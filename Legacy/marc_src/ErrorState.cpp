// ErrorState.cpp
#include "ErrorState.hpp"

namespace Marc {

ErrorState& ErrorState::instance() {
    static ErrorState instance;
    return instance;
}

void ErrorState::set_error(const std::string& message) {
    std::lock_guard<std::mutex> lock(error_mutex);
    last_error = message;
    Logger::instance().log("Error: " + message);
}

const char* ErrorState::get_last_error() {
    std::lock_guard<std::mutex> lock(error_mutex);
    return last_error.c_str();
}

} // namespace Marc