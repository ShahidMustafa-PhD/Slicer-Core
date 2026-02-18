// ==============================================================================
// MarcSLM - SLM Configuration JSON Reader
// ==============================================================================
// Ported from Legacy BuildStyleConfigManager.h (Marc::SlmConfigReader)
// Loads SlmConfig and BuildStyles from JSON files
// ==============================================================================

#pragma once

#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/BuildStyleManager.hpp"

#include <string>
#include <filesystem>

namespace MarcSLM {

/// @brief Reads SlmConfig from JSON files.
/// @details Simple JSON parser using manual string parsing (no nlohmann::json
///          dependency required). For production, consider adding nlohmann::json
///          via vcpkg for robust parsing.
class SlmConfigReader {
public:
    /// @brief Load SlmConfig from a JSON file.
    /// @param filepath Path to the JSON configuration file.
    /// @param config Output configuration struct.
    /// @return true on success, false on failure.
    static bool loadFromFile(const std::string& filepath, SlmConfig& config);

    /// @brief Load SlmConfig from a JSON string.
    /// @param jsonStr JSON string to parse.
    /// @param config Output configuration struct.
    /// @return true on success, false on failure.
    static bool loadFromString(const std::string& jsonStr, SlmConfig& config);
};

} // namespace MarcSLM
