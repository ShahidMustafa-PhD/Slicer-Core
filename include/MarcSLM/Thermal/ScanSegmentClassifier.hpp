// ==============================================================================
// MarcSLM - Scan Segment Classifier (Thermal Segmentation)
// ==============================================================================
// Industrial-grade 22-zone thermal classification for SLM scan vectors.
//
// Algorithm summary (standard SLM Boolean logic):
//   1. Overhang vs. Volume  (Downskin vs. Inskin)
//      - Volume mask  = Intersection(L_n, L_{n-1})
//      - Overhang mask = Difference(L_n, L_{n-1})
//   2. Shells vs. Core
//      - Consecutive inward offsets isolate Shell1, Shell2, Core.
//   3. Contour vs. Hatch
//      - Within each region, a slight inward offset separates the
//        outer boundary ring (Contour) from the inner area (Hatch).
//   4. Final Intersection
//      - Boolean intersection of physical regions and thermal masks
//        maps geometry to the 22 ThermalSegmentType enums.
//
// Uses Clipper2Lib::Paths64 with FillRule::NonZero and JoinType::Miter.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"

#include <clipper2/clipper.h>

#include <vector>

namespace MarcSLM {

// ==============================================================================
// Segmentation Parameters
// ==============================================================================

/// @brief Configuration for the thermal segmentation algorithm.
/// @details All distances are in millimetres.  These are converted to
///          Clipper2 integer coordinates internally using CLIPPER_SCALE.
struct SegmentationParams {
    // Shell offset thicknesses [mm]
    double shell1Thickness = 0.2;   ///< Inward offset for Shell 1 boundary
    double shell2Thickness = 0.2;   ///< Inward offset for Shell 2 boundary

    // Contour ring width [mm]
    double contourWidth    = 0.1;   ///< Inward offset that separates contour from hatch

    // Miter limit for Clipper2 offset operations
    double miterLimit      = 3.0;   ///< Preserves sharp mechanical corners

    // Coordinate scale factor (mm -> Clipper2 int64)
    double clipperScale    = 1e4;   ///< 1 unit = 0.1 um precision (sufficient for SLM)
};

// ==============================================================================
// Classified Region (intermediate Clipper2 representation)
// ==============================================================================

/// @brief A polygonal region tagged with a ThermalSegmentType.
/// @details Produced by the Boolean classification pipeline.
///          Stored as Clipper2 Paths64 for downstream vectorisation.
struct ClassifiedRegion {
    ThermalSegmentType       type;
    Clipper2Lib::Paths64     paths;   ///< Contour + holes (NonZero fill rule)
};

// ==============================================================================
// ClassifiedLayer
// ==============================================================================

/// @brief Per-layer geometry with thermal classification data.
/// @details Extends Marc::Layer with classified scan segments for the
///          SLM machine controller. Each layer's geometry is split into
///          hatch segments and polyline segments, each tagged with a
///          ThermalSegmentType.
struct ClassifiedLayer {
    uint32_t layerNumber    = 0;
    float    layerHeight    = 0.0f;
    float    layerThickness = 0.0f;

    /// @brief Original geometry (hatches, polylines, polygons)
    Marc::Layer geometry;

    /// @brief Classified hatch segments (parallel scan lines)
    std::vector<ScanSegmentHatch> segmentHatches;

    /// @brief Classified polyline segments (contours, perimeters)
    std::vector<ScanSegmentPolyline> segmentPolylines;

    /// @brief Classified polygonal regions (Clipper2 Paths64)
    /// @details Intermediate representation before vectorisation.
    ///          Each entry maps to one of the 22 ThermalSegmentType enums.
    std::vector<ClassifiedRegion> regions;
};

// ==============================================================================
// ScanSegmentClassifier
// ==============================================================================

/// @brief Classifies layer geometry into 22 thermal segments using Clipper2.
///
/// @details Implements the standard SLM Boolean classification pipeline:
///   1. Convert Marc::Layer polylines/polygons to Clipper2 Paths64.
///   2. Compute Volume/Overhang masks by comparing L_n with L_{n-1}.
///   3. Compute Shell1 / Shell2 / Core regions by consecutive inward offsets.
///   4. Within each region, separate Contour ring from Hatch interior.
///   5. Intersect physical regions with thermal masks to produce 22 zones.
///   6. Convert classified regions back to Marc geometry for export.
class ScanSegmentClassifier {
public:
    /// @brief Construct with SLM configuration.
    explicit ScanSegmentClassifier(const SlmConfig& config);

    /// @brief Construct with SLM configuration and custom segmentation params.
    ScanSegmentClassifier(const SlmConfig& config, const SegmentationParams& params);

    /// @brief Classify all layers in a layer stack.
    /// @param layers Input layer geometry from the slicing pipeline.
    /// @return Classified layers with thermal segment assignments.
    [[nodiscard]] std::vector<ClassifiedLayer>
    classifyAll(const std::vector<Marc::Layer>& layers) const;

    /// @brief Classify a single layer (no overhang detection - treats all as volume).
    /// @param layer Input layer geometry.
    /// @return Classified layer with thermal segment assignments.
    [[nodiscard]] ClassifiedLayer
    classifyLayer(const Marc::Layer& layer) const;

private:
    SlmConfig          config_;
    SegmentationParams params_;

    // ---- Coordinate conversion helpers ------------------------------------

    /// @brief Convert mm to Clipper2 int64 coordinate.
    [[nodiscard]] int64_t toClip(double mm) const;

    /// @brief Convert Clipper2 int64 coordinate to mm (float).
    [[nodiscard]] float fromClip(int64_t v) const;

    // ---- Geometry conversion (Marc <-> Clipper2) --------------------------

    /// @brief Convert Marc polylines + polygons of a layer to a single
    ///        Clipper2 Paths64 representing the layer's solid area.
    [[nodiscard]] Clipper2Lib::Paths64
    layerToPaths(const Marc::Layer& layer) const;

    /// @brief Convert a Clipper2 Paths64 region into Marc::Polyline segments
    ///        tagged with the given ThermalSegmentType.
    void pathsToPolylines(const Clipper2Lib::Paths64& paths,
                          ThermalSegmentType type,
                          uint32_t layerNumber,
                          std::vector<ScanSegmentPolyline>& out) const;

    /// @brief Convert a Clipper2 Paths64 region into Marc::Line hatch segments
    ///        tagged with the given ThermalSegmentType.
    void pathsToHatches(const Clipper2Lib::Paths64& paths,
                        ThermalSegmentType type,
                        uint32_t layerNumber,
                        std::vector<ScanSegmentHatch>& out) const;

    // ---- Boolean operations (thin wrappers around Clipper2) ---------------

    /// @brief Clipper2 Intersection: subject AND clip.
    [[nodiscard]] Clipper2Lib::Paths64
    boolIntersect(const Clipper2Lib::Paths64& subject,
                  const Clipper2Lib::Paths64& clip) const;

    /// @brief Clipper2 Difference: subject MINUS clip.
    [[nodiscard]] Clipper2Lib::Paths64
    boolDifference(const Clipper2Lib::Paths64& subject,
                   const Clipper2Lib::Paths64& clip) const;

    /// @brief Clipper2 Union: subject OR clip.
    [[nodiscard]] Clipper2Lib::Paths64
    boolUnion(const Clipper2Lib::Paths64& subject,
              const Clipper2Lib::Paths64& clip) const;

    /// @brief Clipper2 inward (negative) offset.
    [[nodiscard]] Clipper2Lib::Paths64
    offsetInward(const Clipper2Lib::Paths64& paths, double distanceMm) const;

    // ---- Classification pipeline stages -----------------------------------

    /// @brief Classify a single layer with an optional previous layer for
    ///        overhang detection.
    [[nodiscard]] ClassifiedLayer
    classifyLayerImpl(const Marc::Layer& layer,
                      const Clipper2Lib::Paths64* prevLayerPaths) const;

    /// @brief Passthrough classification for existing hatches (preserves tags).
    void classifyExistingHatches(const Marc::Layer& layer,
                                 ClassifiedLayer& classified) const;

    /// @brief Passthrough classification for existing polylines (preserves tags).
    void classifyExistingPolylines(const Marc::Layer& layer,
                                   ClassifiedLayer& classified) const;

    /// @brief Passthrough classification for existing polygons (preserves tags).
    void classifyExistingPolygons(const Marc::Layer& layer,
                                  ClassifiedLayer& classified) const;

    /// @brief Map a GeometryType + BuildStyleID to ThermalSegmentType.
    [[nodiscard]] ThermalSegmentType
    mapToThermalSegment(Marc::GeometryType gtype,
                        Marc::BuildStyleID bstyle) const;
};

} // namespace MarcSLM
