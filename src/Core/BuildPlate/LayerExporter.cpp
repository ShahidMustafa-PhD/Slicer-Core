// ==============================================================================
// MarcSLM - Layer Exporter — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/LayerExporter.hpp"

#include "MarcSLM/Geometry/TriMesh.hpp"

#include <algorithm>
#include <cstdint>

namespace MarcSLM {
namespace BP {

// ==============================================================================
// Public Interface
// ==============================================================================

std::vector<Marc::Layer> LayerExporter::exportAll(
    const std::vector<std::vector<const BuildLayer*>>& objectLayers) const
{
    const auto sorted = collectAndSort(objectLayers);

    std::vector<Marc::Layer> result;
    result.reserve(sorted.size());

    for (std::uint32_t i = 0;
         i < static_cast<std::uint32_t>(sorted.size()); ++i) {
        result.push_back(convertLayer(sorted[i], i));
    }
    return result;
}

// ==============================================================================
// Surface ? Marc type mappings
// ==============================================================================

Marc::GeometryType LayerExporter::surfaceToGeometryType(
    SurfaceType type) noexcept
{
    switch (type) {
        case SurfaceType::Support:
        case SurfaceType::Anchor:
            return Marc::GeometryType::SupportStructure;
        case SurfaceType::Bridge:
            return Marc::GeometryType::OverhangHatch;
        default:
            return Marc::GeometryType::Perimeter;
    }
}

Marc::BuildStyleID LayerExporter::surfaceToBuildStyle(SurfaceType type) noexcept
{
    switch (type) {
        case SurfaceType::Top:
            return Marc::BuildStyleID::CoreContour_UpSkin;
        case SurfaceType::Bottom:
        case SurfaceType::Bridge:
            return Marc::BuildStyleID::CoreContourOverhang_DownSkin;
        case SurfaceType::Support:
        case SurfaceType::Anchor:
            return Marc::BuildStyleID::SupportContour;
        default:
            return Marc::BuildStyleID::CoreContour_Volume;
    }
}

Marc::Polyline LayerExporter::convertPath(const Clipper2Lib::Path64& path,
                                            const Marc::GeometryTag&   tag,
                                            bool                       closePath)
{
    Marc::Polyline polyline;
    polyline.tag = tag;
    polyline.reserve(path.size() + (closePath ? 1u : 0u));

    for (const auto& pt : path) {
        const float x = static_cast<float>(
            static_cast<double>(pt.x) * Geometry::MESH_SCALING_FACTOR);
        const float y = static_cast<float>(
            static_cast<double>(pt.y) * Geometry::MESH_SCALING_FACTOR);
        polyline.points.emplace_back(x, y);
    }

    if (closePath && !polyline.points.empty())
        polyline.points.push_back(polyline.points.front());

    return polyline;
}

// ==============================================================================
// Private Implementation
// ==============================================================================

std::vector<LayerExporter::LayerEntry> LayerExporter::collectAndSort(
    const std::vector<std::vector<const BuildLayer*>>& objectLayers)
{
    std::vector<LayerEntry> entries;
    for (const auto& objLayers : objectLayers) {
        for (const auto* layer : objLayers) {
            if (!layer) continue;
            entries.push_back({layer->printZ(), layer->height(), layer});
        }
    }
    std::sort(entries.begin(), entries.end(),
              [](const LayerEntry& a, const LayerEntry& b) {
                  return a.printZ < b.printZ;
              });
    return entries;
}

Marc::Layer LayerExporter::convertLayer(const LayerEntry&  entry,
                                          std::uint32_t      outputIndex)
{
    Marc::Layer marcLayer(
        outputIndex,
        static_cast<float>(entry.printZ),
        static_cast<float>(entry.thickness));

    if (!entry.layer) return marcLayer;

    for (std::size_t ri = 0; ri < entry.layer->regionCount(); ++ri) {
        const auto* region = entry.layer->getRegion(ri);
        if (region) appendRegion(marcLayer, *region, outputIndex);
    }
    return marcLayer;
}

void LayerExporter::appendRegion(Marc::Layer&            dest,
                                   const BuildLayerRegion& region,
                                   std::uint32_t           layerIdx)
{
    // --- Slice contours (classified surfaces) ----------------------------------
    for (const auto& surf : region.slices) {
        if (!surf.isValid()) continue;

        Marc::GeometryTag tag(surfaceToGeometryType(surf.type),
                              surfaceToBuildStyle(surf.type));
        tag.layerNumber = layerIdx;

        // Outer contour
        dest.polylines.push_back(convertPath(surf.contour, tag, true));

        // Holes (internal boundaries)
        for (const auto& hole : surf.holes) {
            if (hole.size() >= 3)
                dest.polylines.push_back(convertPath(hole, tag, true));
        }
    }

    // --- Support surfaces -------------------------------------------------------
    {
        Marc::GeometryTag supportTag(Marc::GeometryType::SupportStructure,
                                      Marc::BuildStyleID::SupportContour);
        supportTag.layerNumber = layerIdx;

        for (const auto& surf : region.supportSurfaces) {
            if (!surf.isValid()) continue;
            dest.polylines.push_back(convertPath(surf.contour, supportTag, true));
        }
    }
}

} // namespace BP
} // namespace MarcSLM
