// ==============================================================================
// MarcSLM - Scan Segment Classifier Implementation
// ==============================================================================
// Industrial-grade 22-zone thermal classification for SLM scan vectors.
//
// Algorithm (standard SLM Boolean logic using Clipper2):
//   1. Convert each Marc::Layer to Clipper2 Paths64 (solid area).
//   2. Volume mask  = Intersect(L_n, L_{n-1})
//      Overhang mask = Difference(L_n, L_{n-1})
//      First layer is 100% overhang (no previous layer).
//   3. Shell1 = Difference(layerArea, OffsetInward(layerArea, shell1Thickness))
//      Shell2 = Difference(inner1, OffsetInward(inner1, shell2Thickness))
//      Core   = OffsetInward(inner1, shell2Thickness)
//   4. Within each region R:
//        Contour = Difference(R, OffsetInward(R, contourWidth))
//        Hatch   = OffsetInward(R, contourWidth)
//   5. Final assignment:
//        Intersect(Contour_Shell1, volumeMask) -> Shell1Contour_Volume
//        Intersect(Contour_Shell1, overhangMask) -> Shell1Contour_Overhang
//        ...etc for all 22 types.
//   6. Convert Clipper2 regions back to Marc polylines/hatches.
//
// Additionally, any pre-existing hatches/polylines/polygons that already
// carry valid BuildStyleID tags are passed through with tag-based mapping.
// ==============================================================================

#include "MarcSLM/Thermal/ScanSegmentClassifier.hpp"

#include <clipper2/clipper.h>

#include <algorithm>
#include <cmath>
#include <iostream>

namespace MarcSLM {

// ==============================================================================
// Construction
// ==============================================================================

ScanSegmentClassifier::ScanSegmentClassifier(const SlmConfig& config)
    : config_(config)
    , params_()
{
    // Derive shell thicknesses from config if available
    params_.shell1Thickness = config.perimeter_hatch_spacing;
    params_.shell2Thickness = config.perimeter_hatch_spacing;
    params_.contourWidth    = config.beam_diameter;
}

ScanSegmentClassifier::ScanSegmentClassifier(const SlmConfig& config,
                                             const SegmentationParams& params)
    : config_(config)
    , params_(params)
{
}

// ==============================================================================
// Coordinate Conversion
// ==============================================================================

int64_t ScanSegmentClassifier::toClip(double mm) const {
    return static_cast<int64_t>(std::round(mm * params_.clipperScale));
}

float ScanSegmentClassifier::fromClip(int64_t v) const {
    return static_cast<float>(static_cast<double>(v) / params_.clipperScale);
}

// ==============================================================================
// Geometry Conversion: Marc -> Clipper2
// ==============================================================================

Clipper2Lib::Paths64
ScanSegmentClassifier::layerToPaths(const Marc::Layer& layer) const {
    Clipper2Lib::Paths64 paths;

    // Convert polygons (closed paths -> solid area boundaries)
    for (const auto& polygon : layer.polygons) {
        if (polygon.points.size() < 3) continue;
        Clipper2Lib::Path64 path;
        path.reserve(polygon.points.size());
        for (const auto& pt : polygon.points) {
            path.emplace_back(toClip(pt.x), toClip(pt.y));
        }
        paths.push_back(std::move(path));
    }

    // Convert closed polylines (>= 3 points) as polygon boundaries
    for (const auto& polyline : layer.polylines) {
        if (polyline.points.size() < 3) continue;
        Clipper2Lib::Path64 path;
        path.reserve(polyline.points.size());
        for (const auto& pt : polyline.points) {
            path.emplace_back(toClip(pt.x), toClip(pt.y));
        }
        paths.push_back(std::move(path));
    }

    // Union all paths to produce a clean, non-overlapping solid area
    if (!paths.empty()) {
        paths = Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero);
    }

    return paths;
}

// ==============================================================================
// Geometry Conversion: Clipper2 -> Marc
// ==============================================================================

void ScanSegmentClassifier::pathsToPolylines(
    const Clipper2Lib::Paths64& paths,
    ThermalSegmentType type,
    uint32_t layerNumber,
    std::vector<ScanSegmentPolyline>& out) const
{
    if (paths.empty()) return;

    ScanSegmentPolyline seg;
    seg.type = type;

    for (const auto& path : paths) {
        if (path.size() < 2) continue;

        Marc::Polyline polyline;
        polyline.points.reserve(path.size() + 1);  // +1 for closing point

        for (const auto& pt : path) {
            polyline.points.emplace_back(fromClip(pt.x), fromClip(pt.y));
        }
        // Close the polyline (Clipper2 paths are implicitly closed)
        if (!path.empty()) {
            polyline.points.emplace_back(fromClip(path.front().x),
                                         fromClip(path.front().y));
        }

        polyline.tag.type = Marc::GeometryType::Perimeter;
        polyline.tag.layerNumber = layerNumber;
        seg.polylines.push_back(std::move(polyline));
    }

    if (!seg.polylines.empty()) {
        out.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::pathsToHatches(
    const Clipper2Lib::Paths64& paths,
    ThermalSegmentType type,
    uint32_t layerNumber,
    std::vector<ScanSegmentHatch>& out) const
{
    // Hatch regions are stored as closed polygon boundaries.
    // The actual hatch line generation is done later by the HatchGenerator.
    // Here we store the boundary as line segments for the classified output.
    if (paths.empty()) return;

    ScanSegmentHatch seg;
    seg.type = type;

    for (const auto& path : paths) {
        if (path.size() < 2) continue;

        for (size_t i = 0; i < path.size(); ++i) {
            size_t next = (i + 1) % path.size();
            Marc::Line line(
                fromClip(path[i].x), fromClip(path[i].y),
                fromClip(path[next].x), fromClip(path[next].y)
            );
            seg.hatches.push_back(line);
        }
    }

    if (!seg.hatches.empty()) {
        out.push_back(std::move(seg));
    }
}

// ==============================================================================
// Boolean Operations (thin wrappers around Clipper2)
// ==============================================================================

Clipper2Lib::Paths64
ScanSegmentClassifier::boolIntersect(const Clipper2Lib::Paths64& subject,
                                     const Clipper2Lib::Paths64& clip) const {
    return Clipper2Lib::Intersect(subject, clip, Clipper2Lib::FillRule::NonZero);
}

Clipper2Lib::Paths64
ScanSegmentClassifier::boolDifference(const Clipper2Lib::Paths64& subject,
                                      const Clipper2Lib::Paths64& clip) const {
    return Clipper2Lib::Difference(subject, clip, Clipper2Lib::FillRule::NonZero);
}

Clipper2Lib::Paths64
ScanSegmentClassifier::boolUnion(const Clipper2Lib::Paths64& subject,
                                 const Clipper2Lib::Paths64& clip) const {
    return Clipper2Lib::Union(subject, clip, Clipper2Lib::FillRule::NonZero);
}

Clipper2Lib::Paths64
ScanSegmentClassifier::offsetInward(const Clipper2Lib::Paths64& paths,
                                    double distanceMm) const {
    if (paths.empty() || distanceMm <= 0.0) return paths;

    // Negative delta = inward offset (deflation)
    const double delta = -distanceMm * params_.clipperScale;

    return Clipper2Lib::InflatePaths(
        paths,
        delta,
        Clipper2Lib::JoinType::Miter,
        Clipper2Lib::EndType::Polygon,
        params_.miterLimit
    );
}

// ==============================================================================
// Public API: Classify All Layers
// ==============================================================================

std::vector<ClassifiedLayer>
ScanSegmentClassifier::classifyAll(const std::vector<Marc::Layer>& layers) const {
    std::vector<ClassifiedLayer> result;
    result.reserve(layers.size());

    // Pre-compute Clipper2 Paths64 for every layer (used for overhang detection)
    std::vector<Clipper2Lib::Paths64> layerPaths;
    layerPaths.reserve(layers.size());
    for (const auto& layer : layers) {
        layerPaths.push_back(layerToPaths(layer));
    }

    for (size_t i = 0; i < layers.size(); ++i) {
        const Clipper2Lib::Paths64* prevPaths =
            (i > 0) ? &layerPaths[i - 1] : nullptr;

        result.push_back(classifyLayerImpl(layers[i], prevPaths));
    }

    return result;
}

// ==============================================================================
// Public API: Classify Single Layer (no overhang detection)
// ==============================================================================

ClassifiedLayer
ScanSegmentClassifier::classifyLayer(const Marc::Layer& layer) const {
    return classifyLayerImpl(layer, nullptr);
}

// ==============================================================================
// Core Classification Pipeline
// ==============================================================================

ClassifiedLayer
ScanSegmentClassifier::classifyLayerImpl(
    const Marc::Layer& layer,
    const Clipper2Lib::Paths64* prevLayerPaths) const
{
    ClassifiedLayer classified;
    classified.layerNumber    = layer.layerNumber;
    classified.layerHeight    = layer.layerHeight;
    classified.layerThickness = layer.layerThickness;
    classified.geometry       = layer;

    // ------------------------------------------------------------------
    // Stage 0: Pass through any existing tagged geometry
    // ------------------------------------------------------------------
    classifyExistingHatches(layer, classified);
    classifyExistingPolylines(layer, classified);
    classifyExistingPolygons(layer, classified);

    // ------------------------------------------------------------------
    // Stage 1: Convert layer to Clipper2 solid area
    // ------------------------------------------------------------------
    Clipper2Lib::Paths64 currentPaths = layerToPaths(layer);
    if (currentPaths.empty()) {
        return classified;
    }

    // ------------------------------------------------------------------
    // Stage 2: Compute Volume / Overhang (Downskin) thermal masks
    // ------------------------------------------------------------------
    // Volume  = Intersection(L_n, L_{n-1})  -- areas supported by previous layer
    // Overhang = Difference(L_n, L_{n-1})   -- areas over powder (downskin)
    //
    // For the first layer (no previous), everything is overhang.
    // ------------------------------------------------------------------
    Clipper2Lib::Paths64 volumeMask;
    Clipper2Lib::Paths64 overhangMask;

    if (prevLayerPaths != nullptr && !prevLayerPaths->empty()) {
        volumeMask   = boolIntersect(currentPaths, *prevLayerPaths);
        overhangMask = boolDifference(currentPaths, *prevLayerPaths);
    } else {
        // First layer or no previous: entire area is overhang (on powder / build plate)
        overhangMask = currentPaths;
        // volumeMask stays empty
    }

    // ------------------------------------------------------------------
    // Stage 3: Compute Shell1, Shell2, Core by consecutive inward offsets
    // ------------------------------------------------------------------
    //
    // Shell1 = layerArea - OffsetInward(layerArea, shell1Thickness)
    //   This is the outermost ring of material.
    //
    // inner1 = OffsetInward(layerArea, shell1Thickness)
    //   The remaining area after Shell1 is removed.
    //
    // Shell2 = inner1 - OffsetInward(inner1, shell2Thickness)
    //   The second ring of material inside Shell1.
    //
    // Core = OffsetInward(inner1, shell2Thickness)
    //   The central solid region.
    // ------------------------------------------------------------------
    Clipper2Lib::Paths64 inner1 = offsetInward(currentPaths, params_.shell1Thickness);
    Clipper2Lib::Paths64 shell1Region = boolDifference(currentPaths, inner1);

    Clipper2Lib::Paths64 inner2 = offsetInward(inner1, params_.shell2Thickness);
    Clipper2Lib::Paths64 shell2Region = boolDifference(inner1, inner2);

    Clipper2Lib::Paths64 coreRegion = inner2;

    // ------------------------------------------------------------------
    // Stage 4: Within each region, separate Contour from Hatch
    // ------------------------------------------------------------------
    // Contour = Region - OffsetInward(Region, contourWidth)
    //   The thin outer boundary ring where the laser traces the contour.
    //
    // Hatch = OffsetInward(Region, contourWidth)
    //   The interior area filled with parallel scan vectors.
    // ------------------------------------------------------------------

    // Helper lambda: split a region into contour and hatch
    auto splitContourHatch = [this](const Clipper2Lib::Paths64& region)
        -> std::pair<Clipper2Lib::Paths64, Clipper2Lib::Paths64>
    {
        if (region.empty()) return {{}, {}};
        auto hatch   = offsetInward(region, params_.contourWidth);
        auto contour = boolDifference(region, hatch);
        return {contour, hatch};
    };

    auto [shell1Contour, shell1Hatch] = splitContourHatch(shell1Region);
    auto [shell2Contour, shell2Hatch] = splitContourHatch(shell2Region);
    auto [coreContour,   coreHatch]   = splitContourHatch(coreRegion);

    // ------------------------------------------------------------------
    // Stage 5: Final intersection with thermal masks -> 22 segment types
    // ------------------------------------------------------------------
    // Each physical region (Shell1/Shell2/Core x Contour/Hatch) is intersected
    // with Volume and Overhang masks to produce the specific thermal zone.
    //
    // We also generate ContourHatch zones (the contour-hatch transition),
    // which are the hatch areas closest to contours. For simplicity,
    // we assign these the same as contour hatch types.
    // ------------------------------------------------------------------

    struct ZoneMapping {
        const Clipper2Lib::Paths64* region;
        ThermalSegmentType volumeType;
        ThermalSegmentType overhangType;
        bool isContour;  // true = output as polylines, false = output as hatches
    };

    const ZoneMapping zones[] = {
        // Core Contour
        { &coreContour,
          ThermalSegmentType::CoreContour_Volume,
          ThermalSegmentType::CoreContour_Overhang,
          true },
        // Core Hatch
        { &coreHatch,
          ThermalSegmentType::CoreNormalHatch,
          ThermalSegmentType::CoreOverhangHatch,
          false },
        // Shell1 Contour
        { &shell1Contour,
          ThermalSegmentType::HollowShell1Contour_Volume,
          ThermalSegmentType::HollowShell1Contour_Overhang,
          true },
        // Shell1 Hatch
        { &shell1Hatch,
          ThermalSegmentType::HollowShell1NormalHatch,
          ThermalSegmentType::HollowShell1OverhangHatch,
          false },
        // Shell2 Contour
        { &shell2Contour,
          ThermalSegmentType::HollowShell2Contour_Volume,
          ThermalSegmentType::HollowShell2Contour_Overhang,
          true },
        // Shell2 Hatch
        { &shell2Hatch,
          ThermalSegmentType::HollowShell2NormalHatch,
          ThermalSegmentType::HollowShell2OverhangHatch,
          false },
    };

    for (const auto& zone : zones) {
        if (zone.region->empty()) continue;

        // Volume intersection
        if (!volumeMask.empty()) {
            auto volumeResult = boolIntersect(*zone.region, volumeMask);
            if (!volumeResult.empty()) {
                if (zone.isContour) {
                    pathsToPolylines(volumeResult, zone.volumeType,
                                     layer.layerNumber, classified.segmentPolylines);
                } else {
                    pathsToHatches(volumeResult, zone.volumeType,
                                   layer.layerNumber, classified.segmentHatches);
                }

                // Store the classified region
                classified.regions.push_back({zone.volumeType, std::move(volumeResult)});
            }
        }

        // Overhang intersection
        if (!overhangMask.empty()) {
            auto overhangResult = boolIntersect(*zone.region, overhangMask);
            if (!overhangResult.empty()) {
                if (zone.isContour) {
                    pathsToPolylines(overhangResult, zone.overhangType,
                                     layer.layerNumber, classified.segmentPolylines);
                } else {
                    pathsToHatches(overhangResult, zone.overhangType,
                                   layer.layerNumber, classified.segmentHatches);
                }

                classified.regions.push_back({zone.overhangType, std::move(overhangResult)});
            }
        }
    }

    // ------------------------------------------------------------------
    // Stage 5b: Contour-Hatch (transition zone) classification
    // ------------------------------------------------------------------
    // ContourHatch zones are the thin strips at the boundary between
    // contour and hatch. In industrial SLM this is typically the
    // "contour offset" zone. We generate them as the intersection of
    // the full contour ring with an outward-grown hatch boundary.
    //
    // For simplicity we map them to the ContourHatch types.
    // ------------------------------------------------------------------

    struct ContourHatchMapping {
        const Clipper2Lib::Paths64* contourRegion;
        const Clipper2Lib::Paths64* hatchRegion;
        ThermalSegmentType volumeType;
        ThermalSegmentType overhangType;
    };

    const ContourHatchMapping chZones[] = {
        { &coreContour,   &coreHatch,
          ThermalSegmentType::CoreContourHatch,
          ThermalSegmentType::CoreContourHatchOverhang },
        { &shell1Contour, &shell1Hatch,
          ThermalSegmentType::HollowShell1ContourHatch,
          ThermalSegmentType::HollowShell1ContourHatchOverhang },
        { &shell2Contour, &shell2Hatch,
          ThermalSegmentType::HollowShell2ContourHatch,
          ThermalSegmentType::HollowShell2ContourHatchOverhang },
    };

    // ContourHatch = the inner edge of the contour ring, computed as
    // a very thin strip at the interface.  For computational efficiency,
    // we approximate this by offsetting the hatch region outward by a
    // small amount and intersecting with the contour region.
    const double chStripWidth = params_.contourWidth * 0.3;

    for (const auto& chz : chZones) {
        if (chz.contourRegion->empty() || chz.hatchRegion->empty()) continue;

        // Grow hatch slightly outward to create overlap strip
        auto grownHatch = Clipper2Lib::InflatePaths(
            *chz.hatchRegion,
            chStripWidth * params_.clipperScale,
            Clipper2Lib::JoinType::Miter,
            Clipper2Lib::EndType::Polygon,
            params_.miterLimit
        );

        auto chStrip = boolIntersect(*chz.contourRegion, grownHatch);
        if (chStrip.empty()) continue;

        // Volume part
        if (!volumeMask.empty()) {
            auto vol = boolIntersect(chStrip, volumeMask);
            if (!vol.empty()) {
                pathsToPolylines(vol, chz.volumeType,
                                 layer.layerNumber, classified.segmentPolylines);
                classified.regions.push_back({chz.volumeType, std::move(vol)});
            }
        }

        // Overhang part
        if (!overhangMask.empty()) {
            auto oh = boolIntersect(chStrip, overhangMask);
            if (!oh.empty()) {
                pathsToPolylines(oh, chz.overhangType,
                                 layer.layerNumber, classified.segmentPolylines);
                classified.regions.push_back({chz.overhangType, std::move(oh)});
            }
        }
    }

    // ------------------------------------------------------------------
    // Stage 6: Support geometry passthrough
    // ------------------------------------------------------------------
    // Support structures retain their original tags and are mapped
    // directly to ThermalSegmentType::SupportHatch / SupportContourVolume.
    // This is already handled by classifyExistingHatches/Polylines above.

    return classified;
}

// ==============================================================================
// Passthrough Classification (preserves existing tags)
// ==============================================================================

void ScanSegmentClassifier::classifyExistingHatches(
    const Marc::Layer& layer,
    ClassifiedLayer& classified) const
{
    for (const auto& hatch : layer.hatches) {
        ScanSegmentHatch seg;
        seg.type    = mapToThermalSegment(hatch.tag.type, hatch.tag.buildStyle);
        seg.hatches = hatch.lines;
        classified.segmentHatches.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::classifyExistingPolylines(
    const Marc::Layer& layer,
    ClassifiedLayer& classified) const
{
    for (const auto& polyline : layer.polylines) {
        ScanSegmentPolyline seg;
        seg.type = mapToThermalSegment(polyline.tag.type, polyline.tag.buildStyle);

        Marc::Polyline pl;
        pl.points = polyline.points;
        pl.tag    = polyline.tag;
        seg.polylines.push_back(std::move(pl));

        classified.segmentPolylines.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::classifyExistingPolygons(
    const Marc::Layer& layer,
    ClassifiedLayer& classified) const
{
    for (const auto& polygon : layer.polygons) {
        ScanSegmentPolyline seg;
        seg.type = mapToThermalSegment(polygon.tag.type, polygon.tag.buildStyle);

        // Convert polygon to closed polyline
        Marc::Polyline pl;
        pl.points = polygon.points;
        if (!pl.points.empty()) {
            pl.points.push_back(pl.points.front());  // close the polygon
        }
        pl.tag = polygon.tag;
        seg.polylines.push_back(std::move(pl));

        classified.segmentPolylines.push_back(std::move(seg));
    }
}

// ==============================================================================
// BuildStyleID -> ThermalSegmentType Mapping
// ==============================================================================

ThermalSegmentType
ScanSegmentClassifier::mapToThermalSegment(Marc::GeometryType gtype,
                                           Marc::BuildStyleID bstyle) const
{
    switch (bstyle) {
        // ---- Volume (solid interior) ----------------------------------------
        case Marc::BuildStyleID::CoreContour_Volume:
            return ThermalSegmentType::CoreContour_Volume;
        case Marc::BuildStyleID::CoreHatch_Volume:
            return ThermalSegmentType::CoreNormalHatch;
        case Marc::BuildStyleID::Shell1Contour_Volume:
            return ThermalSegmentType::HollowShell1Contour_Volume;
        case Marc::BuildStyleID::Shell1Hatch_Volume:
            return ThermalSegmentType::HollowShell1NormalHatch;
        case Marc::BuildStyleID::Shell2Contour_Volume:
            return ThermalSegmentType::HollowShell2Contour_Volume;
        case Marc::BuildStyleID::Shell2Hatch_Volume:
            return ThermalSegmentType::HollowShell2NormalHatch;

        // ---- UpSkin (treated as Volume for thermal purposes) ----------------
        case Marc::BuildStyleID::CoreContour_UpSkin:
            return ThermalSegmentType::CoreContour_Volume;
        case Marc::BuildStyleID::CoreHatch_UpSkin:
            return ThermalSegmentType::CoreNormalHatch;
        case Marc::BuildStyleID::Shell1Contour_UpSkin:
            return ThermalSegmentType::HollowShell1Contour_Volume;
        case Marc::BuildStyleID::Shell1Hatch_UpSkin:
            return ThermalSegmentType::HollowShell1NormalHatch;

        // ---- DownSkin / Overhang -------------------------------------------
        case Marc::BuildStyleID::CoreContourOverhang_DownSkin:
            return ThermalSegmentType::CoreContour_Overhang;
        case Marc::BuildStyleID::CoreHatchOverhang_DownSkin:
            return ThermalSegmentType::CoreOverhangHatch;
        case Marc::BuildStyleID::Shell1ContourOverhang_DownSkin:
            return ThermalSegmentType::HollowShell1Contour_Overhang;
        case Marc::BuildStyleID::Shell1HatchOverhang_DownSkin:
            return ThermalSegmentType::HollowShell1OverhangHatch;

        // ---- Hollow Shell 1 ------------------------------------------------
        case Marc::BuildStyleID::HollowShell1Contour:
            return ThermalSegmentType::HollowShell1Contour_Volume;
        case Marc::BuildStyleID::HollowShell1ContourHatch:
            return ThermalSegmentType::HollowShell1ContourHatch;
        case Marc::BuildStyleID::HollowShell1ContourHatchOverhang:
            return ThermalSegmentType::HollowShell1ContourHatchOverhang;

        // ---- Hollow Shell 2 ------------------------------------------------
        case Marc::BuildStyleID::HollowShell2Contour:
            return ThermalSegmentType::HollowShell2Contour_Volume;
        case Marc::BuildStyleID::HollowShell2ContourHatch:
            return ThermalSegmentType::HollowShell2ContourHatch;
        case Marc::BuildStyleID::HollowShell2ContourHatchOverhang:
            return ThermalSegmentType::HollowShell2ContourHatchOverhang;

        // ---- Support -------------------------------------------------------
        case Marc::BuildStyleID::SupportStructure:
            return ThermalSegmentType::SupportHatch;
        case Marc::BuildStyleID::SupportContour:
            return ThermalSegmentType::SupportContourVolume;

        default:
            break;
    }

    // Fallback: classify based on GeometryType
    switch (gtype) {
        case Marc::GeometryType::CoreHatch:
            return ThermalSegmentType::CoreNormalHatch;
        case Marc::GeometryType::OverhangHatch:
            return ThermalSegmentType::CoreOverhangHatch;
        case Marc::GeometryType::Perimeter:
            return ThermalSegmentType::CoreContour_Volume;
        case Marc::GeometryType::SupportStructure:
            return ThermalSegmentType::ExternalSupports;
        case Marc::GeometryType::InfillPattern:
            return ThermalSegmentType::CoreContourHatch;
        default:
            return ThermalSegmentType::CoreContour_Volume;
    }
}

} // namespace MarcSLM
