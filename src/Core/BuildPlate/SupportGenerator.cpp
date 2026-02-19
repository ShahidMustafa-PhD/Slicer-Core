// ==============================================================================
// MarcSLM - Support Generator — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/SupportGenerator.hpp"

#include "MarcSLM/Geometry/TriMesh.hpp"   // MESH_SCALING_FACTOR

#include <clipper2/clipper.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// Public Interface
// ==============================================================================

void SupportGenerator::generate(
    const std::vector<BuildLayer*>& layers,
    const OverhangMap&              overhangs,
    const SlmConfig&                config) const
{
    if (overhangs.empty() || layers.empty()) return;

    // Stage 1: grid-sample overhang areas ? pillar XY positions
    const auto positions = placePillars(overhangs,
                                        config.support_material_pillar_spacing);
    if (positions.empty()) return;

    // Stage 2: resolve vertical extents
    const auto pillars = resolveExtents(layers, positions,
                                         config.support_material_pillar_size);
    if (pillars.empty()) return;

    // Stage 3: stamp circular geometry into the layer stack
    stampGeometry(layers, pillars, config.support_material_pillar_size);
}

// ==============================================================================
// Stage 1 — Pillar placement
// ==============================================================================

std::map<std::size_t, std::vector<Clipper2Lib::Point64>>
SupportGenerator::placePillars(const OverhangMap& overhangs,
                                 double             pillarSpacing)
{
    std::map<std::size_t, std::vector<Clipper2Lib::Point64>> result;

    const int64_t scaledSpacing = static_cast<int64_t>(
        pillarSpacing / Geometry::MESH_SCALING_FACTOR);

    if (scaledSpacing <= 0) return result;

    for (const auto& [regionId, layerOverhangs] : overhangs) {
        // Collect and union all overhang paths for this region
        Clipper2Lib::Paths64 allOverhangs;
        for (const auto& [, paths] : layerOverhangs) {
            allOverhangs.insert(allOverhangs.end(), paths.begin(), paths.end());
        }
        if (allOverhangs.empty()) continue;

        Clipper2Lib::Clipper64 clipper;
        clipper.AddSubject(allOverhangs);
        Clipper2Lib::Paths64 unified;
        clipper.Execute(Clipper2Lib::ClipType::Union,
                        Clipper2Lib::FillRule::NonZero, unified);

        // Grid-sample within each unified polygon
        std::vector<Clipper2Lib::Point64> regionPillars;
        for (const auto& path : unified) {
            int64_t minX = std::numeric_limits<int64_t>::max();
            int64_t maxX = std::numeric_limits<int64_t>::min();
            int64_t minY = std::numeric_limits<int64_t>::max();
            int64_t maxY = std::numeric_limits<int64_t>::min();

            for (const auto& pt : path) {
                minX = std::min(minX, pt.x);
                maxX = std::max(maxX, pt.x);
                minY = std::min(minY, pt.y);
                maxY = std::max(maxY, pt.y);
            }

            for (int64_t y = minY; y <= maxY; y += scaledSpacing) {
                for (int64_t x = minX; x <= maxX; x += scaledSpacing) {
                    const Clipper2Lib::Point64 testPt(x, y);
                    if (Clipper2Lib::PointInPolygon(testPt, path) !=
                        Clipper2Lib::PointInPolygonResult::IsOutside) {
                        regionPillars.push_back(testPt);
                    }
                }
            }
        }

        if (!regionPillars.empty()) {
            result[regionId] = std::move(regionPillars);
        }
    }

    return result;
}

// ==============================================================================
// Stage 2 — Vertical extent resolution
// ==============================================================================

std::map<std::size_t, std::vector<SupportPillar>>
SupportGenerator::resolveExtents(
    const std::vector<BuildLayer*>&                            layers,
    const std::map<std::size_t,
                   std::vector<Clipper2Lib::Point64>>& positions,
    double                                              pillarSize)
{
    std::map<std::size_t, std::vector<SupportPillar>> result;
    (void)pillarSize;   // reserved for future extent-narrowing by bounding box

    for (const auto& [regionId, points] : positions) {
        std::vector<SupportPillar> regionPillars;

        for (const auto& pt : points) {
            SupportPillar pillar;
            pillar.x          = static_cast<double>(pt.x) * Geometry::MESH_SCALING_FACTOR;
            pillar.y          = static_cast<double>(pt.y) * Geometry::MESH_SCALING_FACTOR;
            pillar.radiusBase = pillarSize / 2.0;
            pillar.radiusTop  = pillar.radiusBase * 0.8;
            pillar.topLayer   = 0;
            pillar.bottomLayer = 0;

            bool objectHit = false;

            // Walk from top to bottom to find the contiguous gap
            for (int li = static_cast<int>(layers.size()) - 1; li >= 0; --li) {
                const std::size_t layerIdx = static_cast<std::size_t>(li);
                const auto* layer = layers[layerIdx];

                if (!layer || regionId >= layer->regionCount()) continue;

                const auto* layerRegion = layer->getRegion(regionId);
                if (!layerRegion) continue;

                bool pointInside = false;
                for (const auto& surf : layerRegion->slices) {
                    if (!surf.isValid()) continue;
                    if (Clipper2Lib::PointInPolygon(pt, surf.contour) !=
                        Clipper2Lib::PointInPolygonResult::IsOutside) {
                        pointInside = true;
                        break;
                    }
                }

                if (!pointInside) {
                    if (objectHit) {
                        // We are in the gap below the first mesh encounter
                        if (pillar.topLayer == 0) {
                            pillar.topLayer = layerIdx;
                        }
                        pillar.bottomLayer = layerIdx;
                    }
                } else {
                    // Inside the mesh — record and reset if a new gap starts
                    if (pillar.topLayer > 0) {
                        regionPillars.push_back(pillar);
                        pillar.topLayer    = 0;
                        pillar.bottomLayer = 0;
                    }
                    objectHit = true;
                }
            }

            // Flush any open pillar reaching the build plate
            if (pillar.topLayer > 0) {
                regionPillars.push_back(pillar);
            }
        }

        if (!regionPillars.empty()) {
            result[regionId] = std::move(regionPillars);
        }
    }

    return result;
}

// ==============================================================================
// Stage 3 — Geometry stamping
// ==============================================================================

void SupportGenerator::stampGeometry(
    const std::vector<BuildLayer*>&                            layers,
    const std::map<std::size_t, std::vector<SupportPillar>>& pillarsByRegion,
    double                                                     pillarSize)
{
    const double baseRadius = pillarSize / 2.0;
    const double topRadius  = baseRadius * 0.2;

    for (std::size_t li = 0; li < layers.size(); ++li) {
        auto* layer = layers[li];
        if (!layer) continue;

        for (const auto& [regionId, pillars] : pillarsByRegion) {
            if (regionId >= layer->regionCount()) continue;
            auto* layerRegion = layer->getRegion(regionId);
            if (!layerRegion) continue;

            for (const auto& pillar : pillars) {
                if (li < pillar.bottomLayer || li > pillar.topLayer) continue;

                const std::size_t span = (pillar.topLayer > pillar.bottomLayer)
                    ? (pillar.topLayer - pillar.bottomLayer)
                    : 1;
                const double heightFraction =
                    static_cast<double>(li - pillar.bottomLayer) /
                    static_cast<double>(span);

                const double radius = taperedRadius(heightFraction,
                                                    baseRadius, topRadius);

                const int64_t scaledR  =
                    static_cast<int64_t>(radius / Geometry::MESH_SCALING_FACTOR);
                const int64_t cx =
                    static_cast<int64_t>(pillar.x / Geometry::MESH_SCALING_FACTOR);
                const int64_t cy =
                    static_cast<int64_t>(pillar.y / Geometry::MESH_SCALING_FACTOR);

                const int sides =
                    std::max(8, static_cast<int>(radius / 0.1) * 2 + 8);

                ClassifiedSurface supportSurf;
                supportSurf.contour = makeCircle(cx, cy, scaledR, sides);
                supportSurf.type    = SurfaceType::Support;

                if (supportSurf.isValid()) {
                    layerRegion->supportSurfaces.push_back(
                        std::move(supportSurf));
                }
            }
        }
    }
}

// ==============================================================================
// Geometry Helpers
// ==============================================================================

Clipper2Lib::Path64 SupportGenerator::makeCircle(int64_t cx, int64_t cy,
                                                   int64_t radius,
                                                   int     sides) noexcept
{
    Clipper2Lib::Path64 path;
    path.reserve(static_cast<std::size_t>(sides));
    for (int s = 0; s < sides; ++s) {
        const double angle = 2.0 * M_PI * s / sides;
        path.emplace_back(
            cx + static_cast<int64_t>(static_cast<double>(radius) * std::cos(angle)),
            cy + static_cast<int64_t>(static_cast<double>(radius) * std::sin(angle)));
    }
    return path;
}

double SupportGenerator::taperedRadius(double heightFraction,
                                        double baseRadius,
                                        double topRadius) noexcept
{
    // Linear taper in the middle third; flat base/top regions for stability.
    if (heightFraction < 0.2) {
        return baseRadius;
    }
    if (heightFraction > 0.8) {
        return topRadius;
    }
    const double t = (heightFraction - 0.2) / 0.6;
    return baseRadius - (baseRadius - topRadius) * t;
}

} // namespace BuildPlate
} // namespace MarcSLM
