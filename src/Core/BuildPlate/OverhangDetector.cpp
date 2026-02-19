// ==============================================================================
// MarcSLM - Overhang Detector — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/OverhangDetector.hpp"

#include "MarcSLM/Geometry/TriMesh.hpp"   // MESH_SCALING_FACTOR

#include <clipper2/clipper.h>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// Public Interface
// ==============================================================================

OverhangMap OverhangDetector::detect(
    const std::vector<BuildLayer*>& layers,
    double                          supportAngle) const
{
    OverhangMap result;

    for (std::size_t li = 1; li < layers.size(); ++li) {
        const auto* layer      = layers[li];
        const auto* lowerLayer = layers[li - 1];

        if (!layer || !lowerLayer) continue;

        const int64_t offset = computeOffset(layer->height(), supportAngle);

        for (std::size_t ri = 0; ri < layer->regionCount(); ++ri) {
            const auto* layerRegion = layer->getRegion(ri);
            const auto* lowerRegion = lowerLayer->getRegion(ri);

            if (!layerRegion || !lowerRegion) continue;
            if (layerRegion->slices.empty())  continue;

            // Current layer contours
            Clipper2Lib::Paths64 current;
            for (const auto& surf : layerRegion->slices) {
                if (surf.isValid()) current.push_back(surf.contour);
            }

            // Lower layer contours, expanded by the critical offset
            Clipper2Lib::Paths64 lowerPaths = regionContours(*lowerRegion);

            const Clipper2Lib::Paths64 supportedArea =
                Clipper2Lib::InflatePaths(
                    lowerPaths,
                    static_cast<double>(offset),
                    Clipper2Lib::JoinType::Miter,
                    Clipper2Lib::EndType::Polygon);

            // Unsupported = current – expanded lower
            const Clipper2Lib::Paths64 unsupported =
                Clipper2Lib::BooleanOp(
                    Clipper2Lib::ClipType::Difference,
                    Clipper2Lib::FillRule::NonZero,
                    current, supportedArea);

            if (!unsupported.empty()) {
                result[ri][li] = unsupported;
            }
        }
    }

    return result;
}

// ==============================================================================
// Private Helpers
// ==============================================================================

int64_t OverhangDetector::computeOffset(double layerHeight,
                                         double supportAngle) noexcept
{
    if (supportAngle <= 0.0 || supportAngle >= 90.0) return 0;

    const double tanAngle = std::tan(supportAngle * M_PI / 180.0);
    if (tanAngle <= 0.0) return 0;

    const double maxLateralDist = layerHeight / tanAngle;
    return static_cast<int64_t>(
        maxLateralDist / Geometry::MESH_SCALING_FACTOR);
}

Clipper2Lib::Paths64 OverhangDetector::regionContours(
    const BuildLayerRegion& region)
{
    Clipper2Lib::Paths64 paths;
    for (const auto& surf : region.slices) {
        if (surf.isValid()) paths.push_back(surf.contour);
    }
    return paths;
}

} // namespace BuildPlate
} // namespace MarcSLM
