// ==============================================================================
// MarcSLM - Layer Exporter
// ==============================================================================
// Responsibility: Convert the internal BuildLayer / BuildLayerRegion
//                 representation into the Marc::Layer wire format that the
//                 rest of the pipeline (path planning, binary export, SVG) uses.
//
// This class is the single point responsible for the coordinate-unit
// conversion from Clipper2 integer space back to millimetres.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"
#include "MarcSLM/Core/MarcFormat.hpp"

#include <cstdint>
#include <vector>

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// LayerExporter
// ==============================================================================

/// @brief Converts processed BuildLayer data to Marc::Layer format.
///
/// @details Ported and isolated from the legacy `BuildPlate::exportLayers`.
///          Responsibilities:
///            - Merge layers from all objects, sorted by printZ.
///            - Convert ClassifiedSurface contours + holes to Marc::Polyline.
///            - Map SurfaceType ? GeometryType + BuildStyleID.
///            - Scale coordinates from Clipper2 integer units to mm.
///            - Close every contour by appending the first point.
///
/// ### Coordinate convention
///   Clipper2 integer ? mm:  `x_mm = static_cast<double>(pt.x) * MESH_SCALING_FACTOR`
///
/// ### Thread Safety
///   Stateless: `exportAll()` only reads input data and is fully re-entrant.
class LayerExporter {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    LayerExporter() = default;
    ~LayerExporter() = default;

    LayerExporter(const LayerExporter&)            = default;
    LayerExporter& operator=(const LayerExporter&) = default;
    LayerExporter(LayerExporter&&) noexcept        = default;
    LayerExporter& operator=(LayerExporter&&) noexcept = default;

    // =========================================================================
    // Primary Interface
    // =========================================================================

    /// @brief Convert all build layers (from all objects) into Marc::Layer.
    ///
    /// @param objectLayers  Per-object collections of BuildLayer pointers.
    ///                      The outer vector is indexed by object, the inner
    ///                      by layer.  Null object-level vectors are skipped.
    ///
    /// @return  Vector of Marc::Layer sorted by ascending printZ.
    [[nodiscard]] std::vector<Marc::Layer> exportAll(
        const std::vector<std::vector<const BuildLayer*>>& objectLayers) const;

    // =========================================================================
    // Helpers (public to allow unit-testing of sub-operations)
    // =========================================================================

    /// @brief Map a SurfaceType to a Marc::GeometryType.
    [[nodiscard]] static Marc::GeometryType surfaceToGeometryType(
        SurfaceType type) noexcept;

    /// @brief Map a SurfaceType to a Marc::BuildStyleID.
    [[nodiscard]] static Marc::BuildStyleID surfaceToBuildStyle(
        SurfaceType type) noexcept;

    /// @brief Convert a single Clipper2 integer path to a Marc::Polyline.
    ///
    /// @param path        Source path in Clipper2 integer coordinates.
    /// @param tag         Metadata (type, buildStyle, layerNumber).
    /// @param closePath   If true, the first point is appended to close the loop.
    [[nodiscard]] static Marc::Polyline convertPath(
        const Clipper2Lib::Path64& path,
        const Marc::GeometryTag&   tag,
        bool                       closePath = true);

private:
    // =========================================================================
    // Implementation Helpers
    // =========================================================================

    /// @brief Flat layer entry used when merging multi-object stacks.
    struct LayerEntry {
        double            printZ    = 0.0;
        double            thickness = 0.0;
        const BuildLayer* layer     = nullptr;
    };

    /// @brief Collect and sort all LayerEntry objects from the input.
    [[nodiscard]] static std::vector<LayerEntry> collectAndSort(
        const std::vector<std::vector<const BuildLayer*>>& objectLayers);

    /// @brief Convert one BuildLayer into a Marc::Layer.
    [[nodiscard]] static Marc::Layer convertLayer(
        const LayerEntry& entry, std::uint32_t outputIndex);

    /// @brief Append Marc::Polylines for one BuildLayerRegion to a Marc::Layer.
    static void appendRegion(Marc::Layer&            dest,
                              const BuildLayerRegion& region,
                              std::uint32_t           layerIdx);
};

} // namespace BuildPlate
} // namespace MarcSLM
