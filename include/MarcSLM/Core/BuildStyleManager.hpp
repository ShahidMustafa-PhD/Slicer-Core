// ==============================================================================
// MarcSLM - Build Style Configuration Manager
// ==============================================================================
// Ported from Legacy BuildStyleConfigManager.h
// Manages laser parameter sets (BuildStyles) loaded from JSON
// ==============================================================================

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>

namespace MarcSLM {

/// @brief A single laser parameter set for SLM printing.
/// @details Each BuildStyle defines the complete laser configuration for a
///          specific thermal segment type. Loaded from JSON configuration.
///          Ported from Legacy BuildStyle struct.
struct BuildStyle {
    int         id = 0;                  ///< Unique style identifier (1-22)
    std::string name;                    ///< Human-readable name
    std::string description;             ///< Description of the style
    int         laserId = 0;             ///< Laser unit identifier
    int         laserMode = 0;           ///< Laser operation mode
    double      laserPower = 0.0;        ///< Laser power [W]
    double      laserFocus = 0.0;        ///< Laser focus offset [mm]
    double      laserSpeed = 0.0;        ///< Scan speed [mm/s]
    double      hatchSpacing = 0.0;      ///< Hatch line spacing [mm]
    double      layerThickness = 0.0;    ///< Layer thickness [mm]
    double      pointDistance = 0.0;     ///< Point-to-point distance [mm]
    int         pointDelay = 0;          ///< Delay at points [µs]
    int         pointExposureTime = 0;   ///< Exposure time at points [µs]
    double      jumpSpeed = 0.0;         ///< Jump (non-lasing) speed [mm/s]
    double      jumpDelay = 0.0;         ///< Delay after jump [ms]
    double      hatchSize = 0.0;         ///< Hatch island/pattern size [mm]
};

/// @brief Manages a collection of BuildStyles loaded from JSON files.
/// @details Provides load/save functionality and lookup by ID.
///          Ported from Legacy ConfigurationManager class.
class BuildStyleManager {
public:
    std::vector<BuildStyle> styles; ///< All loaded build styles

    /// @brief Load build styles from a JSON file.
    /// @param filepath Path to JSON file containing "buildStyles" array.
    /// @return true on success, false on failure.
    bool loadFromFile(const std::filesystem::path& filepath);

    /// @brief Save build styles to a JSON file.
    /// @param filepath Output JSON file path.
    /// @return true on success, false on failure.
    bool saveToFile(const std::filesystem::path& filepath) const;

    /// @brief Get all loaded styles.
    [[nodiscard]] const std::vector<BuildStyle>& getStyles() const noexcept {
        return styles;
    }

    /// @brief Find a build style by ID.
    /// @param id Style identifier to search for.
    /// @return Pointer to style, or nullptr if not found.
    [[nodiscard]] const BuildStyle* findById(int id) const noexcept;

    /// @brief Check if any styles are loaded.
    [[nodiscard]] bool empty() const noexcept { return styles.empty(); }

    /// @brief Get the number of loaded styles.
    [[nodiscard]] size_t count() const noexcept { return styles.size(); }
};

} // namespace MarcSLM
