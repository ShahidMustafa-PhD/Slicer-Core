// ==============================================================================
// MarcSLM - Error State (Thread-safe Singleton)
// ==============================================================================
// Ported from Legacy ErrorState.hpp
// ==============================================================================

#pragma once

#include <string>
#include <mutex>

namespace MarcSLM {

/// @brief Thread-safe singleton for tracking the last error.
/// @details Ported from Legacy Marc::ErrorState.
class ErrorState {
public:
    static ErrorState& instance();

    void set_error(const std::string& message);
    const char* get_last_error();

private:
    ErrorState() = default;
    ErrorState(const ErrorState&) = delete;
    ErrorState& operator=(const ErrorState&) = delete;

    std::mutex  error_mutex;
    std::string last_error;
};

} // namespace MarcSLM
