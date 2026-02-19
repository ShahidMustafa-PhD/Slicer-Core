// ==============================================================================
// MarcSLM - SLM Print Engine
// ==============================================================================
// Ported from Legacy SlmPrint.hpp/cpp
// Orchestrates: load ? slice ? hatch ? classify ? export
// ==============================================================================

#pragma once

#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/BuildStyleManager.hpp"
#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/MarcFile.hpp"
#include "MarcSLM/Geometry/MeshProcessor.hpp"
#include "MarcSLM/Thermal/ScanSegmentClassifier.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"
#include "MarcSLM/PathPlanning/HatchGenerator.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace MarcSLM {

/// @brief Complete SLM slicer print engine.
/// @details Orchestrates the full pipeline from 3D mesh loading through
///          slicing, hatch generation, thermal classification, and export
///          to .marc binary and SVG visualisation formats.
///
///          Pipeline:
///          1. loadMesh()      ? Load 3D model via Assimp ? TriMesh (repair)
///          2. slice()         ? Uniform/adaptive Z-plane slicing (triangle-plane intersection)
///          3. generatePaths() ? Perimeter contours + hatch infill
///          4. classify()      ? Thermal segment classification
///          5. exportMarc()    ? Binary .marc file
///          6. exportSVG()     ? SVG visualisation per layer
///
///          Ported from Legacy Marc::SlmPrint + Marc::MarcAPI.
class SlmPrint {
public:
    /// @brief Progress callback: (message, percent 0-100).
    using ProgressCallback = std::function<void(const char*, int)>;

    SlmPrint();
    ~SlmPrint() = default;

    SlmPrint(const SlmPrint&) = delete;
    SlmPrint& operator=(const SlmPrint&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Load SLM configuration from a JSON file.
    bool loadSlmConfig(const std::filesystem::path& configPath);

    /// @brief Load build styles from a JSON file.
    bool loadBuildStyles(const std::filesystem::path& stylesPath);

    /// @brief Set SLM configuration directly.
    void setSlmConfig(const SlmConfig& config);

    /// @brief Get current SLM configuration (read-only).
    [[nodiscard]] const SlmConfig& getConfig() const noexcept { return config_; }

    /// @brief Get loaded build styles (read-only).
    [[nodiscard]] const BuildStyleManager& getBuildStyles() const noexcept {
        return buildStyles_;
    }

    // =========================================================================
    // Model Loading
    // =========================================================================

    /// @brief Load a 3D mesh from file.
    /// @param filePath Path to STL, 3MF, OBJ, etc.
    /// @return true on success.
    bool loadMesh(const std::string& filePath);

    /// @brief Check if a valid mesh is loaded.
    [[nodiscard]] bool hasMesh() const noexcept;

    // =========================================================================
    // Slicing Pipeline
    // =========================================================================

    /// @brief Perform uniform slicing using config layer_thickness.
    /// @return true on success.
    bool slice();

    /// @brief Perform uniform slicing with explicit layer thickness.
    bool sliceUniform(float layerThickness);

    /// @brief Perform adaptive slicing.
    bool sliceAdaptive(float minHeight, float maxHeight, float maxError = 0.05f);

    /// @brief Get the number of sliced layers.
    [[nodiscard]] size_t layerCount() const noexcept { return layers_.size(); }

    /// @brief Get a specific layer (read-only).
    [[nodiscard]] const Marc::Layer* getLayer(size_t index) const;

    // =========================================================================
    // Path Generation
    // =========================================================================

    /// @brief Generate hatch infill and perimeter paths for all layers.
    /// @details Adds hatch lines and contour polylines to each layer.
    bool generatePaths();

    // =========================================================================
    // Thermal Classification
    // =========================================================================

    /// @brief Classify all scan vectors by thermal segment type.
    /// @return Vector of classified layers.
    [[nodiscard]] std::vector<ClassifiedLayer> classify() const;

    // =========================================================================
    // Export
    // =========================================================================

    /// @brief Export to .marc binary file.
    /// @param outputPath Output file path.
    /// @return true on success.
    bool exportMarc(const std::string& outputPath);

    /// @brief Export SVG visualisation for all layers.
    /// @param outputDir Directory for SVG files.
    /// @return true on success.
    bool exportSVG(const std::string& outputDir);

    /// @brief Export SVG with thermal segment colouring.
    /// @param outputDir Directory for SVG files.
    /// @return true on success.
    bool exportSegmentSVG(const std::string& outputDir);

    /// @brief Execute the complete pipeline: slice ? paths ? classify ? export.
    /// @param outputDir Output directory for all files.
    /// @return true on success.
    bool processAndExport(const std::string& outputDir);

    // =========================================================================
    // Progress Callback
    // =========================================================================

    void setProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }

private:
    SlmConfig                        config_;
    BuildStyleManager                buildStyles_;
    Geometry::MeshProcessor          meshProcessor_;
    std::vector<Marc::Layer>         layers_;
    ProgressCallback                 progressCb_;

    void reportProgress(const char* message, int percent);
};

} // namespace MarcSLM
