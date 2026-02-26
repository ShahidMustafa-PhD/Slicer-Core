// ==============================================================================
// MarcSLM - Thermal Region Hatcher Implementation
// ==============================================================================
// Generates actual parallel scan vectors inside classified thermal region
// boundaries, implementing PySLM's BasicHatcher, StripeHatcher, and
// IslandHatcher strategies.
// ==============================================================================

#include "MarcSLM/Thermal/ThermalRegionHatcher.hpp"
#include "MarcSLM/Thermal/ClipperBoolean.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MarcSLM {
namespace Thermal {

// ==============================================================================
// Parameter Resolution
// ==============================================================================
// Maps each ThermalSegmentType to its hatching strategy and spacing.
// PySLM logic:
//   - Core hatch regions      ? Island strategy, normal spacing
//   - Shell hatch regions     ? Basic strategy, slightly finer spacing
//   - Contour regions         ? No infill hatches (they are boundary paths)
//   - ContourHatch transition ? Basic strategy, fine spacing
//   - Support                 ? Basic strategy, coarse spacing
// ==============================================================================

RegionHatchParams
ThermalRegionHatcher::resolveParams(ThermalSegmentType segType,
                                    uint32_t layerIndex) const
{
    RegionHatchParams p;
    p.islandW = islandW_;
    p.islandH = islandH_;
    p.overlap = endpointOverlap_;

    // Compute per-layer angle: base + layerIndex * 67�
    const double layerAngle = baseAngle_ + layerIndex * layerRotation_;

    switch (segType) {
        // ---- Core hatch: Island strategy (PySLM default) ------------------
        case ThermalSegmentType::CoreNormalHatch:
        case ThermalSegmentType::CoreOverhangHatch:
            p.spacing  = hatchSpacing_;
            p.angle    = std::fmod(layerAngle, 360.0);
            p.strategy = HatchStrategy::Island;
            break;

        // ---- Shell1 hatch: Basic strategy, same spacing -------------------
        case ThermalSegmentType::HollowShell1NormalHatch:
        case ThermalSegmentType::HollowShell1OverhangHatch:
            p.spacing  = hatchSpacing_;
            p.angle    = std::fmod(layerAngle, 360.0);
            p.strategy = HatchStrategy::Basic;
            break;

        // ---- Shell2 hatch: Basic strategy, same spacing -------------------
        case ThermalSegmentType::HollowShell2NormalHatch:
        case ThermalSegmentType::HollowShell2OverhangHatch:
            p.spacing  = hatchSpacing_;
            p.angle    = std::fmod(layerAngle, 360.0);
            p.strategy = HatchStrategy::Basic;
            break;

        // ---- ContourHatch transition: Basic, finer spacing ----------------
        case ThermalSegmentType::CoreContourHatch:
        case ThermalSegmentType::CoreContourHatchOverhang:
        case ThermalSegmentType::HollowShell1ContourHatch:
        case ThermalSegmentType::HollowShell1ContourHatchOverhang:
        case ThermalSegmentType::HollowShell2ContourHatch:
        case ThermalSegmentType::HollowShell2ContourHatchOverhang:
            p.spacing  = hatchSpacing_ * 0.8;  // Finer for transition zone
            p.angle    = std::fmod(layerAngle, 360.0);
            p.strategy = HatchStrategy::Basic;
            break;

        // ---- Support hatch: Basic, coarser spacing ------------------------
        case ThermalSegmentType::SupportHatch:
        case ThermalSegmentType::ExternalSupports:
            p.spacing  = hatchSpacing_ * 1.5;  // Coarser for support
            p.angle    = std::fmod(layerAngle + 45.0, 360.0);
            p.strategy = HatchStrategy::Basic;
            break;

        // ---- Contour types: no infill hatches (they are boundary paths) ---
        case ThermalSegmentType::CoreContour_Volume:
        case ThermalSegmentType::CoreContour_Overhang:
        case ThermalSegmentType::HollowShell1Contour_Volume:
        case ThermalSegmentType::HollowShell1Contour_Overhang:
        case ThermalSegmentType::HollowShell2Contour_Volume:
        case ThermalSegmentType::HollowShell2Contour_Overhang:
        case ThermalSegmentType::SupportContourVolume:
        case ThermalSegmentType::PointSequence:
            p.spacing  = 0.0;  // Sentinel: no hatches for contour types
            p.strategy = HatchStrategy::Basic;
            break;

        default:
            p.spacing  = hatchSpacing_;
            p.angle    = std::fmod(layerAngle, 360.0);
            p.strategy = HatchStrategy::Basic;
            break;
    }

    return p;
}

// ==============================================================================
// Primary Entry Point
// ==============================================================================

std::vector<Marc::Line>
ThermalRegionHatcher::hatchRegion(const Clipper2Lib::Paths64& regionPaths,
                                  ThermalSegmentType segType,
                                  uint32_t layerIndex) const
{
    if (regionPaths.empty()) return {};

    auto params = resolveParams(segType, layerIndex);

    // Contour types produce no infill hatches
    if (params.spacing <= 0.0) return {};

    std::vector<Marc::Line> result;

    switch (params.strategy) {
        case HatchStrategy::Basic:
            result = hatchBasic(regionPaths, params);
            break;
        case HatchStrategy::Stripe:
            result = hatchStripe(regionPaths, params);
            break;
        case HatchStrategy::Island:
            result = hatchIsland(regionPaths, params);
            break;
    }

    if (params.overlap > 0.0 && !result.empty()) {
        applyOverlap(result);
    }

    if (result.size() > 1) {
        sortAlternating(result);
    }

    return result;
}

// ==============================================================================
// Basic Hatcher (PySLM Hatcher.hatch)
// ==============================================================================

std::vector<Marc::Line>
ThermalRegionHatcher::hatchBasic(const Clipper2Lib::Paths64& region,
                                 const RegionHatchParams& p) const
{
    double xMin, yMin, xMax, yMax;
    computeBoundingBox(region, xMin, yMin, xMax, yMax);

    // Expand bounding box for rotated coverage
    const double margin = p.spacing * 2.0;
    xMin -= margin;  yMin -= margin;
    xMax += margin;  yMax += margin;

    auto lines = generateParallelLines(xMin, yMin, xMax, yMax,
                                       p.spacing, p.angle);
    return clipLinesToRegion(lines, region);
}

// ==============================================================================
// Stripe Hatcher (PySLM StripeHatcher)
// ==============================================================================

std::vector<Marc::Line>
ThermalRegionHatcher::hatchStripe(const Clipper2Lib::Paths64& region,
                                  const RegionHatchParams& p) const
{
    double xMin, yMin, xMax, yMax;
    computeBoundingBox(region, xMin, yMin, xMax, yMax);

    std::vector<Marc::Line> allLines;
    const double stripeW = (p.stripeW > 0.0) ? p.stripeW : 10.0;

    // Divide along X into vertical stripes
    for (double sx = xMin; sx < xMax; sx += stripeW) {
        double sxMax = std::min(sx + stripeW, xMax);

        // Build a clip rectangle for this stripe
        Clipper2Lib::Path64 stripeRect = {
            {toClip(sx),    toClip(yMin)},
            {toClip(sxMax), toClip(yMin)},
            {toClip(sxMax), toClip(yMax)},
            {toClip(sx),    toClip(yMax)}
        };

        // Intersect stripe rectangle with the region
        Clipper2Lib::Paths64 stripePaths = {stripeRect};
        auto clippedRegion = ClipperBoolean::intersect(stripePaths, region);

        if (clippedRegion.empty()) continue;

        // Generate hatches for this stripe
        const double margin = p.spacing * 2.0;
        auto lines = generateParallelLines(sx - margin, yMin - margin,
                                           sxMax + margin, yMax + margin,
                                           p.spacing, p.angle);
        auto clipped = clipLinesToRegion(lines, clippedRegion);
        allLines.insert(allLines.end(), clipped.begin(), clipped.end());
    }

    return allLines;
}

// ==============================================================================
// Island Hatcher (PySLM IslandHatcher � checkerboard)
// ==============================================================================

std::vector<Marc::Line>
ThermalRegionHatcher::hatchIsland(const Clipper2Lib::Paths64& region,
                                  const RegionHatchParams& p) const
{
    double xMin, yMin, xMax, yMax;
    computeBoundingBox(region, xMin, yMin, xMax, yMax);

    std::vector<Marc::Line> allLines;

    int cellRow = 0;
    for (double y = yMin; y < yMax; y += p.islandH, ++cellRow) {
        int cellCol = 0;
        for (double x = xMin; x < xMax; x += p.islandW, ++cellCol) {
            double cxMax = std::min(x + p.islandW, xMax);
            double cyMax = std::min(y + p.islandH, yMax);

            // Skip degenerate cells
            if ((cxMax - x) < p.spacing || (cyMax - y) < p.spacing) continue;

            // Checkerboard angle: alternate 0� / 90� offset from base angle
            double cellAngle = p.angle + (((cellRow + cellCol) % 2 == 0) ? 0.0 : 90.0);

            // Build the island cell rectangle in Clipper2 coords
            Clipper2Lib::Path64 cellRect = {
                {toClip(x),    toClip(y)},
                {toClip(cxMax), toClip(y)},
                {toClip(cxMax), toClip(cyMax)},
                {toClip(x),    toClip(cyMax)}
            };

            // Intersect cell with the actual thermal region
            Clipper2Lib::Paths64 cellPaths = {cellRect};
            auto cellRegion = ClipperBoolean::intersect(cellPaths, region);
            if (cellRegion.empty()) continue;

            // Generate and clip hatches for this cell
            const double margin = p.spacing * 2.0;
            auto lines = generateParallelLines(x - margin, y - margin,
                                               cxMax + margin, cyMax + margin,
                                               p.spacing, cellAngle);
            auto clipped = clipLinesToRegion(lines, cellRegion);
            allLines.insert(allLines.end(), clipped.begin(), clipped.end());
        }
    }

    return allLines;
}

// ==============================================================================
// Parallel Line Generation
// ==============================================================================

std::vector<Marc::Line>
ThermalRegionHatcher::generateParallelLines(double xMin, double yMin,
                                            double xMax, double yMax,
                                            double spacing,
                                            double angleDeg) const
{
    std::vector<Marc::Line> lines;
    if (spacing <= 0.0) return lines;

    const double angleRad = angleDeg * M_PI / 180.0;
    const double cosA = std::cos(angleRad);
    const double sinA = std::sin(angleRad);

    const double dx = xMax - xMin;
    const double dy = yMax - yMin;
    const double diag = std::sqrt(dx * dx + dy * dy);
    const double cx = (xMin + xMax) * 0.5;
    const double cy = (yMin + yMax) * 0.5;

    const int numLines = static_cast<int>(std::ceil(diag / spacing)) + 2;

    lines.reserve(static_cast<size_t>(numLines));

    for (int i = -numLines / 2; i <= numLines / 2; ++i) {
        const double offset = i * spacing;

        // Point on the sweep line perpendicular to hatch direction
        const double px = cx + offset * (-sinA);
        const double py = cy + offset * cosA;

        // Extend along hatch direction to cover full diagonal
        lines.emplace_back(
            static_cast<float>(px - diag * cosA),
            static_cast<float>(py - diag * sinA),
            static_cast<float>(px + diag * cosA),
            static_cast<float>(py + diag * sinA));
    }

    return lines;
}

// ==============================================================================
// Line-Region Clipping (Clipper2 open-path intersection)
// ==============================================================================

std::vector<Marc::Line>
ThermalRegionHatcher::clipLinesToRegion(const std::vector<Marc::Line>& lines,
                                       const Clipper2Lib::Paths64& region) const
{
    std::vector<Marc::Line> result;
    if (lines.empty() || region.empty()) return result;

    // -------------------------------------------------------------------------
    // Two-step clipping algorithm (matches ScanVectorClipper approach):
    //   Step 1: Intersect open scan lines with outer boundary paths (CCW)
    //   Step 2: Difference the result with hole paths (CW)
    //
    // The region paths come from LayerPolygonExtractor::extract() which
    // produces CCW outers and CW holes after a Clipper2 Union.
    // Clipper2Lib::IsPositive() returns true for CCW paths (outers).
    // -------------------------------------------------------------------------

    // Separate outer boundaries (CCW) from holes (CW)
    Clipper2Lib::Paths64 outers;
    Clipper2Lib::Paths64 holes;
    for (const auto& path : region) {
        if (path.size() < 3) continue;
        if (Clipper2Lib::IsPositive(path)) {
            outers.push_back(path);   // CCW = outer boundary
        } else {
            holes.push_back(path);    // CW  = hole
        }
    }

    if (outers.empty()) return result;

    // Convert Marc::Lines to Clipper2 open subject paths
    Clipper2Lib::Paths64 openSubject;
    openSubject.reserve(lines.size());
    for (const auto& line : lines) {
        Clipper2Lib::Path64 path;
        path.push_back({toClip(line.a.x), toClip(line.a.y)});
        path.push_back({toClip(line.b.x), toClip(line.b.y)});
        openSubject.push_back(std::move(path));
    }

    // Step 1: Intersect with outer boundaries only
    Clipper2Lib::Clipper64 clipper1;
    clipper1.AddOpenSubject(openSubject);
    clipper1.AddClip(outers);

    Clipper2Lib::Paths64 closedSol1;
    Clipper2Lib::Paths64 contourClipped;
    clipper1.Execute(Clipper2Lib::ClipType::Intersection,
                     Clipper2Lib::FillRule::NonZero,
                     closedSol1, contourClipped);

    if (contourClipped.empty()) return result;

    // Step 2: Subtract holes (if any)
    Clipper2Lib::Paths64 finalPaths;
    if (!holes.empty()) {
        Clipper2Lib::Clipper64 clipper2;
        clipper2.AddOpenSubject(contourClipped);
        clipper2.AddClip(holes);

        Clipper2Lib::Paths64 closedSol2;
        clipper2.Execute(Clipper2Lib::ClipType::Difference,
                         Clipper2Lib::FillRule::NonZero,
                         closedSol2, finalPaths);
    } else {
        finalPaths = std::move(contourClipped);
    }

    // Convert back to Marc::Line
    result.reserve(finalPaths.size());
    for (const auto& path : finalPaths) {
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            result.emplace_back(
                static_cast<float>(toMm(path[i].x)),
                static_cast<float>(toMm(path[i].y)),
                static_cast<float>(toMm(path[i + 1].x)),
                static_cast<float>(toMm(path[i + 1].y)));
        }
    }

    return result;
}

// ==============================================================================
// Endpoint Overlap Extension
// ==============================================================================

void ThermalRegionHatcher::applyOverlap(std::vector<Marc::Line>& lines) const
{
    const float overlap = static_cast<float>(endpointOverlap_);
    if (overlap <= 0.0f) return;

    for (auto& line : lines) {
        const float dx = line.b.x - line.a.x;
        const float dy = line.b.y - line.a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-9f) continue;

        const float nx = dx / len;
        const float ny = dy / len;

        line.a.x -= nx * overlap;
        line.a.y -= ny * overlap;
        line.b.x += nx * overlap;
        line.b.y += ny * overlap;
    }
}

// ==============================================================================
// Alternating Scan Direction Sort
// ==============================================================================

void ThermalRegionHatcher::sortAlternating(std::vector<Marc::Line>& lines) const
{
    if (lines.size() < 2) return;

    std::sort(lines.begin(), lines.end(),
        [](const Marc::Line& a, const Marc::Line& b) {
            const float midYa = (a.a.y + a.b.y) * 0.5f;
            const float midYb = (b.a.y + b.b.y) * 0.5f;
            return midYa < midYb;
        });

    for (size_t i = 1; i < lines.size(); i += 2) {
        std::swap(lines[i].a, lines[i].b);
    }
}

// ==============================================================================
// Bounding Box Computation
// ==============================================================================

void ThermalRegionHatcher::computeBoundingBox(
    const Clipper2Lib::Paths64& paths,
    double& xMin, double& yMin,
    double& xMax, double& yMax) const
{
    xMin = yMin =  std::numeric_limits<double>::max();
    xMax = yMax = -std::numeric_limits<double>::max();

    for (const auto& path : paths) {
        for (const auto& pt : path) {
            const double x = toMm(pt.x);
            const double y = toMm(pt.y);
            xMin = std::min(xMin, x);
            yMin = std::min(yMin, y);
            xMax = std::max(xMax, x);
            yMax = std::max(yMax, y);
        }
    }
}

} // namespace Thermal
} // namespace MarcSLM
