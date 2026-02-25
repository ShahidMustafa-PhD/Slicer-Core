// ==============================================================================
// MarcSLM - Scan Segment Classifier (Thermal Segmentation Orchestrator)
// ==============================================================================
// Orchestrates the full PySLM-style thermal classification pipeline:
//
//   Stage 1: LayerPolygonExtractor  - Marc::Layer -> Clipper2 Paths64
//   Stage 2: ThermalMaskGenerator   - Volume / Overhang masks
//   Stage 3: ShellDecomposer        - Shell1 / Shell2 / Core
//   Stage 4: ContourHatchSplitter   - Contour / Hatch per region
//   Stage 5: RegionClassifier       - Final intersection -> 22 types
//
// Parallelism: classifyAll() uses TBB parallel_for over the layer stack.
//              Each layer is independently classifiable once the Paths64
//              array for all layers has been pre-computed (Stage 1).
//
// Memory safety:
//   - No raw owning pointers; all data in std::vector / Clipper2 containers.
//   - ClassifiedLayer is move-constructible for zero-copy pipeline transfer.
//   - All pipeline stage classes are stateless or const-only.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"
#include "MarcSLM/Thermal/ClipperBoolean.hpp"
#include "MarcSLM/Thermal/LayerPolygonExtractor.hpp"
#include "MarcSLM/Thermal/ThermalMaskGenerator.hpp"
#include "MarcSLM/Thermal/ShellDecomposer.hpp"
#include "MarcSLM/Thermal/ContourHatchSplitter.hpp"
#include "MarcSLM/Thermal/RegionClassifier.hpp"
#include "MarcSLM/Thermal/ThermalRegionHatcher.hpp"

#include <clipper2/clipper.h>

#include <vector>

namespace MarcSLM {

// ==============================================================================
// Segmentation Parameters
// ==============================================================================

/// @brief Configuration for the thermal segmentation algorithm.
/// @details All distances are in millimetres.  Converted to Clipper2 integer
///          coordinates internally using clipperScale.
struct SegmentationParams {
    double shell1Thickness        = 0.2;   ///< Inward offset for Shell 1 [mm]
    double shell2Thickness        = 0.2;   ///< Inward offset for Shell 2 [mm]
    double contourWidth           = 0.1;   ///< Contour ring width [mm]
    double contourHatchFraction   = 0.3;   ///< ContourHatch strip fraction of contourWidth
    double miterLimit             = 3.0;   ///< Miter limit for sharp corners
    double clipperScale           = 1e4;   ///< mm -> Clipper2 int64 scale factor
    bool   enableParallel         = true;  ///< Use TBB parallel_for over layers
    bool   generateHatchVectors   = true;  ///< Generate actual parallel scan vectors
    double endpointOverlap        = 0.0;   ///< Hatch endpoint overlap [mm]
};

// ==============================================================================
// Classified Region (intermediate Clipper2 representation)
// ==============================================================================

/// @brief A polygonal region tagged with a ThermalSegmentType.
struct ClassifiedRegion {
    ThermalSegmentType   type;
    Clipper2Lib::Paths64 paths;
};

// ==============================================================================
// ClassifiedLayer
// ==============================================================================

/// @brief Per-layer geometry with thermal classification data.
struct ClassifiedLayer {
    uint32_t layerNumber    = 0;
    float    layerHeight    = 0.0f;
    float    layerThickness = 0.0f;

    Marc::Layer geometry;                              ///< Original geometry
    std::vector<ScanSegmentHatch>    segmentHatches;   ///< Classified hatches
    std::vector<ScanSegmentPolyline> segmentPolylines; ///< Classified polylines
    std::vector<ClassifiedRegion>    regions;           ///< Clipper2 regions

    ClassifiedLayer() = default;
    ClassifiedLayer(ClassifiedLayer&&) = default;
    ClassifiedLayer& operator=(ClassifiedLayer&&) = default;
    ClassifiedLayer(const ClassifiedLayer&) = default;
    ClassifiedLayer& operator=(const ClassifiedLayer&) = default;
};

// ==============================================================================
// ScanSegmentClassifier
// ==============================================================================

/// @brief Orchestrates the full 22-zone thermal classification pipeline
///        with integrated scan vector generation.
class ScanSegmentClassifier {
public:
    explicit ScanSegmentClassifier(const SlmConfig& config);
    ScanSegmentClassifier(const SlmConfig& config, const SegmentationParams& params);

    /// @brief Classify all layers (TBB-parallelised).
    [[nodiscard]] std::vector<ClassifiedLayer>
    classifyAll(const std::vector<Marc::Layer>& layers) const;

    /// @brief Classify a single layer (no overhang detection).
    [[nodiscard]] ClassifiedLayer
    classifyLayer(const Marc::Layer& layer) const;

private:
    SlmConfig          config_;
    SegmentationParams params_;

    [[nodiscard]] ClassifiedLayer
    classifyLayerImpl(const Marc::Layer& layer,
                      const Clipper2Lib::Paths64& currentPaths,
                      const Clipper2Lib::Paths64* prevPaths) const;

    static void passExistingHatches(const Marc::Layer& layer,
                                    ClassifiedLayer& out);
    static void passExistingPolylines(const Marc::Layer& layer,
                                      ClassifiedLayer& out);
    static void passExistingPolygons(const Marc::Layer& layer,
                                     ClassifiedLayer& out);

    [[nodiscard]] static ThermalSegmentType
    mapToThermalSegment(Marc::GeometryType gtype,
                        Marc::BuildStyleID bstyle) noexcept;
};

} // namespace MarcSLM
