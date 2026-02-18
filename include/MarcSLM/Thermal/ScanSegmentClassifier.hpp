// ==============================================================================
// MarcSLM - Scan Segment Classifier (Thermal Segmentation)
// ==============================================================================
// Ported from Legacy scanSegments.hpp/cpp
// Classifies layer geometry into ThermalSegmentType categories
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"

#include <vector>

namespace MarcSLM {

/// @brief Per-layer geometry with thermal classification data.
/// @details Extends Marc::Layer with classified scan segments for the
///          SLM machine controller. Each layer's geometry is split into
///          hatch segments and polyline segments, each tagged with a
///          ThermalSegmentType.
struct ClassifiedLayer {
    uint32_t layerNumber = 0;
    float    layerHeight = 0.0f;
    float    layerThickness = 0.0f;

    /// @brief Original geometry (hatches, polylines, polygons)
    Marc::Layer geometry;

    /// @brief Classified hatch segments (parallel scan lines)
    std::vector<ScanSegmentHatch> segmentHatches;

    /// @brief Classified polyline segments (contours, perimeters)
    std::vector<ScanSegmentPolyline> segmentPolylines;
};

/// @brief Classifies layer geometry into thermal segments.
/// @details Iterates through all layers and their geometry, assigning a
///          ThermalSegmentType to each group based on the geometry's role
///          (perimeter, hatch, support, etc.) and surface condition
///          (volume vs. overhang).
///
///          Ported from Legacy Marc::scanSegments.
class ScanSegmentClassifier {
public:
    /// @brief Construct with SLM configuration.
    explicit ScanSegmentClassifier(const SlmConfig& config);

    /// @brief Classify all layers in a layer stack.
    /// @param layers Input layer geometry from the slicing pipeline.
    /// @return Classified layers with thermal segment assignments.
    [[nodiscard]] std::vector<ClassifiedLayer>
    classifyAll(const std::vector<Marc::Layer>& layers) const;

    /// @brief Classify a single layer.
    /// @param layer Input layer geometry.
    /// @return Classified layer with thermal segment assignments.
    [[nodiscard]] ClassifiedLayer
    classifyLayer(const Marc::Layer& layer) const;

private:
    SlmConfig config_;

    /// @brief Classify hatches within a layer.
    void classifyHatches(const Marc::Layer& layer,
                        ClassifiedLayer& classified) const;

    /// @brief Classify polylines (contours/perimeters) within a layer.
    void classifyPolylines(const Marc::Layer& layer,
                          ClassifiedLayer& classified) const;

    /// @brief Classify polygons (closed regions) within a layer.
    void classifyPolygons(const Marc::Layer& layer,
                         ClassifiedLayer& classified) const;

    /// @brief Map a GeometryType + BuildStyleID to ThermalSegmentType.
    [[nodiscard]] ThermalSegmentType
    mapToThermalSegment(Marc::GeometryType gtype,
                       Marc::BuildStyleID bstyle) const;
};

} // namespace MarcSLM
