// ==============================================================================
// MarcSLM - Error State Implementation
// ==============================================================================

#include "MarcSLM/Core/ErrorState.hpp"
#include "MarcSLM/Core/Logger.hpp"

namespace MarcSLM {

ErrorState& ErrorState::instance() {
    static ErrorState inst;
    return inst;
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

} // namespace MarcSLM
