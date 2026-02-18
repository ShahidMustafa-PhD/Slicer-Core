// ==============================================================================
// MarcSLM - Top-Level API
// ==============================================================================
// Ported from Legacy MarcAPI.hpp/cpp
// Provides the public interface for the SLM slicer engine
// ==============================================================================

#pragma once

#include "MarcSLM/Core/InternalModel.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/SlmPrint.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MarcSLM {

/// @brief Error code type.
using MarcErrorCode = int;

constexpr MarcErrorCode MARC_S_OK      = 0x00000000;
constexpr MarcErrorCode MARC_E_FAIL    = 0x80004005;
constexpr MarcErrorCode MARC_E_INVALID = 0x80070057;
constexpr MarcErrorCode MARC_E_NOTIMPL = 0x80004001;

/// @brief Top-level API for the MarcSLM slicer.
/// @details Provides model loading, configuration, build-plate arrangement,
///          slicing, and export operations. The primary entry point for
///          external applications.
///
///          Usage:
///          1. Construct with bed dimensions
///          2. setModels() with file paths and transforms
///          3. set_config_json() with build configuration
///          4. exportSlmFile() to run the full pipeline
///
///          Ported from Legacy Marc::MarcAPI.
class MarcAPI {
public:
    using PathList         = std::vector<std::filesystem::path>;
    using StringList       = std::vector<std::string>;
    using ProgressCallback = std::function<void(const char*, int)>;

    /// @brief Construct with build plate dimensions.
    /// @param bed_width  Build plate width [mm].
    /// @param bed_depth  Build plate depth [mm].
    /// @param spacing    Minimum spacing between parts [mm].
    MarcAPI(float bed_width, float bed_depth, float spacing = 5.0f);
    ~MarcAPI();

    // =========================================================================
    // Model Management
    // =========================================================================

    /// @brief Set the list of models to process.
    MarcErrorCode setModels(const std::vector<InternalModel>& models);

    /// @brief Get the current internal model list.
    [[nodiscard]] std::vector<InternalModel> getInternalModels() const;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set the JSON build configuration file.
    MarcErrorCode set_config_json(const std::filesystem::path& filePath);

    /// @brief Load and update the model (mesh loading + arrangement).
    MarcErrorCode updateModel();

    // =========================================================================
    // Geometric Transforms
    // =========================================================================

    MarcErrorCode rotateX(float angleDeg);
    MarcErrorCode rotateY(float angleDeg);
    MarcErrorCode rotateZ(float angleDeg);
    MarcErrorCode scale(float factor);
    MarcErrorCode alignXY(float px, float py);

    // =========================================================================
    // Pipeline Execution
    // =========================================================================

    /// @brief Run the full pipeline: load ? slice ? hatch ? classify ? export.
    MarcErrorCode exportSlmFile();

    /// @brief Arrange models on the build plate.
    MarcErrorCode arrangeBuildPlate();

    // =========================================================================
    // Progress Callback
    // =========================================================================

    void registerProgressCallback(ProgressCallback cb);

private:
    float bed_width_;
    float bed_depth_;
    float spacing_;

    std::vector<InternalModel>          models_;
    std::vector<std::filesystem::path>  modelPaths_;
    std::filesystem::path               configFileJson_;

    std::unique_ptr<SlmPrint>           slmPrint_;
    ProgressCallback                    progressCb_;

    void reportProgress(const char* message, int percent);
};

} // namespace MarcSLM
