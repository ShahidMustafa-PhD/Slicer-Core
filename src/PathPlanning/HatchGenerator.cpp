// ==============================================================================
// MarcSLM - Hatch Generator Implementation
// ==============================================================================
// Ported from Legacy slmHatches + FillRectilinear + FillSLMisland logic
// Uses Clipper2 for robust line-polygon clipping
// ==============================================================================

#include "MarcSLM/PathPlanning/HatchGenerator.hpp"
#include "MarcSLM/Core/Types.hpp"

#include <cmath>
#include <algorithm>
#include <map>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MarcSLM {
namespace PathPlanning {

// ==============================================================================
// Construction
// ==============================================================================

HatchGenerator::HatchGenerator(const SlmConfig& config)
    : hatchSpacing_(config.hatch_spacing)
    , hatchAngle_(config.hatch_angle)
    , islandWidth_(config.island_width)
    , islandHeight_(config.island_height)
    , endpointOverlap_(0.0)
    , layerRotation_(67.0) {
}

// ==============================================================================
// Layer Angle Computation
// ==============================================================================

double HatchGenerator::layerAngle(size_t layerIndex) const {
    // Industry-standard 67° rotation per layer for SLM
    double angle = hatchAngle_ + layerIndex * layerRotation_;
    return std::fmod(angle, 360.0);
}

// ==============================================================================
// Primary Hatch Generation (Clipper2 Path64 input)
// ==============================================================================

std::vector<Marc::Line>
HatchGenerator::generateHatches(const Clipper2Lib::Path64& contour,
                                const Clipper2Lib::Paths64& holes,
                                double angle) const {
    if (contour.size() < 3) return {};

    const double effectiveAngle = (angle < 0.0) ? hatchAngle_ : angle;

    // Compute bounding box of contour in mm
    double xMin = std::numeric_limits<double>::max();
    double yMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMax = std::numeric_limits<double>::lowest();

    for (const auto& pt : contour) {
        double x = MarcSLM::Core::clipperUnitsToMm(pt.x);
        double y = MarcSLM::Core::clipperUnitsToMm(pt.y);
        xMin = std::min(xMin, x);
        yMin = std::min(yMin, y);
        xMax = std::max(xMax, x);
        yMax = std::max(yMax, y);
    }

    // Expand bounding box slightly to ensure full coverage
    // (matches Legacy bounding_box expansion logic)
    const double margin = hatchSpacing_ * 2.0;
    xMin -= margin;
    yMin -= margin;
    xMax += margin;
    yMax += margin;

    // Generate parallel lines covering the bounding box
    auto lines = generateParallelLines(xMin, yMin, xMax, yMax,
                                       hatchSpacing_, effectiveAngle);

    // Clip to contour boundary
    auto clipped = clipLinesToPolygon(lines, contour, holes);

    // Apply endpoint overlap if configured
    if (endpointOverlap_ > 0.0) {
        applyEndpointOverlap(clipped);
    }

    return clipped;
}

// ==============================================================================
// Hatch Generation with Island Mode Switch
// ==============================================================================

std::vector<Marc::Line>
HatchGenerator::generateHatches(const Clipper2Lib::Path64& contour,
                                const Clipper2Lib::Paths64& holes,
                                double angle,
                                bool useIslands) const {
    if (useIslands) {
        return generateIslandHatches(contour, holes);
    }
    return generateHatches(contour, holes, angle);
}

// ==============================================================================
// Marc::Polygon Overload
// ==============================================================================

std::vector<Marc::Line>
HatchGenerator::generateHatches(const Marc::Polygon& polygon, double angle) const {
    if (polygon.points.size() < 3) return {};

    // Convert Marc::Polygon to Clipper2 Path64
    Clipper2Lib::Path64 contour;
    contour.reserve(polygon.points.size());
    for (const auto& pt : polygon.points) {
        Clipper2Lib::Point64 p;
        p.x = MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.x));
        p.y = MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.y));
        contour.push_back(p);
    }

    return generateHatches(contour, {}, angle);
}

// ==============================================================================
// Island / Checkerboard Hatching
// (Ported from Legacy FillSLMisland::_fill_surface_single)
// ==============================================================================

std::vector<Marc::Line>
HatchGenerator::generateIslandHatches(const Clipper2Lib::Path64& contour,
                                       const Clipper2Lib::Paths64& holes) const {
    if (contour.size() < 3) return {};

    // Compute bounding box in mm from the contour's Clipper2-unit coordinates.
    double xMin = std::numeric_limits<double>::max();
    double yMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMax = std::numeric_limits<double>::lowest();

    for (const auto& pt : contour) {
        double x = MarcSLM::Core::clipperUnitsToMm(pt.x);
        double y = MarcSLM::Core::clipperUnitsToMm(pt.y);
        xMin = std::min(xMin, x);
        yMin = std::min(yMin, y);
        xMax = std::max(xMax, x);
        yMax = std::max(yMax, y);
    }

    // Build the full solid-region clip paths:
    //   contour (CCW) + holes (CW) — all in Clipper2 integer units.
    // FillRule::NonZero then correctly classifies the interior minus holes.
    Clipper2Lib::Paths64 solidRegion;
    solidRegion.reserve(holes.size() + 1);
    solidRegion.push_back(contour);
    for (const auto& hole : holes) {
        if (hole.size() >= 3) {
            solidRegion.push_back(hole);
        }
    }

    std::vector<Marc::Line> allHatches;

    // Iterate over the island grid.
    int cellRow = 0;
    for (double y = yMin; y < yMax; y += islandHeight_, ++cellRow) {
        int cellCol = 0;
        for (double x = xMin; x < xMax; x += islandWidth_, ++cellCol) {
            double cellXMax = std::min(x + islandWidth_, xMax);
            double cellYMax = std::min(y + islandHeight_, yMax);

            // Skip degenerate cells
            if (cellXMax - x < hatchSpacing_ || cellYMax - y < hatchSpacing_)
                continue;

            // Checkerboard hatch angle: 0° / 90° alternating
            double cellAngle = ((cellRow + cellCol) % 2 == 0) ? 0.0 : 90.0;

            // Build this cell's rectangle in Clipper2 integer units
            Clipper2Lib::Path64 cellRect = makeRectanglePath(x, y, cellXMax, cellYMax);

            // Intersect cell rectangle with the solid region (contour - holes).
            // Using NonZero fill rule for correct winding handling.
            Clipper2Lib::Clipper64 polyClipper;
            polyClipper.AddSubject({cellRect});
            polyClipper.AddClip(solidRegion);

            Clipper2Lib::Paths64 cellSolidRegion;
            polyClipper.Execute(Clipper2Lib::ClipType::Intersection,
                                Clipper2Lib::FillRule::NonZero,
                                cellSolidRegion);

            if (cellSolidRegion.empty()) continue;

            // Generate parallel hatch lines covering this cell (in mm)
            auto cellLines = generateParallelLines(
                x, y, cellXMax, cellYMax,
                hatchSpacing_, cellAngle);

            // Clip cell lines against the cell's solid region.
            // cellSolidRegion paths are in Clipper2 integer units from
            // the Execute above.  clipLinesToPolygon expects pre-scaled
            // Clipper2 paths for the clip region.
            //
            // The output of the Execute above is a flat Paths64 (no PolyTree).
            // For non-self-intersecting cell regions we can use each path
            // as the clip boundary directly — but to handle any holes that
            // were sliced across a cell boundary, we iterate per sub-region.
            for (const auto& subRegion : cellSolidRegion) {
                if (subRegion.size() < 3) continue;

                // For each sub-region polygon: clip lines against it alone.
                // Pass empty holes because the cellSolidRegion paths already
                // had the holes subtracted by the Clipper2 Intersection above.
                auto clipped = clipLinesToPolygon(cellLines, subRegion, {});

                if (endpointOverlap_ > 0.0) {
                    applyEndpointOverlap(clipped);
                }

                allHatches.insert(allHatches.end(), clipped.begin(), clipped.end());
            }
        }
    }

    return allHatches;
}

// ==============================================================================
// Parallel Line Generation
// ==============================================================================

std::vector<Marc::Line>
HatchGenerator::generateParallelLines(double xMin, double yMin,
                                       double xMax, double yMax,
                                       double spacing, double angleDeg) const {
    std::vector<Marc::Line> lines;

    if (spacing <= 0.0) return lines;

    const double angleRad = angleDeg * M_PI / 180.0;
    const double cosA = std::cos(angleRad);
    const double sinA = std::sin(angleRad);

    // Compute the diagonal of the bounding box for sweep distance
    const double dx = xMax - xMin;
    const double dy = yMax - yMin;
    const double diag = std::sqrt(dx * dx + dy * dy);
    const double cx = (xMin + xMax) * 0.5;
    const double cy = (yMin + yMax) * 0.5;

    // Sweep perpendicular to the hatch direction
    // Number of lines to fully cover the diagonal in perpendicular direction
    const int numLines = static_cast<int>(std::ceil(diag / spacing)) + 2;

    for (int i = -numLines / 2; i <= numLines / 2; ++i) {
        double offset = i * spacing;

        // Line perpendicular to sweep direction, centered at (cx, cy)
        // Direction along angle: (cosA, sinA)
        // Perpendicular: (-sinA, cosA)
        double px = cx + offset * (-sinA);
        double py = cy + offset * cosA;

        // Extend line along the angle direction to cover full bounding box
        float x1 = static_cast<float>(px - diag * cosA);
        float y1 = static_cast<float>(py - diag * sinA);
        float x2 = static_cast<float>(px + diag * cosA);
        float y2 = static_cast<float>(py + diag * sinA);

        lines.emplace_back(x1, y1, x2, y2);
    }

    return lines;
}

// ==============================================================================
// Clipper2-Based Line Clipping
// ==============================================================================

std::vector<Marc::Line>
HatchGenerator::clipLinesToPolygon(const std::vector<Marc::Line>& lines,
                                   const Clipper2Lib::Path64& contour,
                                   const Clipper2Lib::Paths64& holes) const {
    std::vector<Marc::Line> result;

    if (lines.empty() || contour.size() < 3) return result;

    // -------------------------------------------------------------------------
    // Build the clip region: outer contour + hole paths.
    //
    // Convention (enforced by PolyTree output in makeExPolygons and by
    // Clipper2 Union with NonZero fill rule):
    //   - contour: CCW winding  ? winding contribution = +1
    //   - holes:   CW  winding  ? winding contribution = -1
    //
    // FillRule::NonZero classifies a point as "inside" (filled) when its
    // accumulated winding number is non-zero.  Therefore:
    //   - Inside contour only:            winding = +1  ? filled  ?
    //   - Inside contour AND inside hole: winding = 0   ? void    ?
    //
    // This correctly clips hatch lines so they stop at hole boundaries.
    // -------------------------------------------------------------------------
    Clipper2Lib::Paths64 clipPaths;
    clipPaths.reserve(holes.size() + 1);
    clipPaths.push_back(contour);
    for (const auto& hole : holes) {
        if (hole.size() >= 3) {
            clipPaths.push_back(hole);
        }
    }

    // Convert hatch lines (in mm) to Clipper2 open subject paths.
    Clipper2Lib::Paths64 openSubject;
    openSubject.reserve(lines.size());
    for (const auto& line : lines) {
        Clipper2Lib::Path64 path;
        path.push_back({
            MarcSLM::Core::mmToClipperUnits(static_cast<double>(line.a.x)),
            MarcSLM::Core::mmToClipperUnits(static_cast<double>(line.a.y))
        });
        path.push_back({
            MarcSLM::Core::mmToClipperUnits(static_cast<double>(line.b.x)),
            MarcSLM::Core::mmToClipperUnits(static_cast<double>(line.b.y))
        });
        openSubject.push_back(std::move(path));
    }

    // NOTE: contour and hole paths are already in Clipper2 integer units
    // (encoded by the caller via mmToClipperUnits before passing here).
    // No additional scaling is required.
    Clipper2Lib::Clipper64 clipper;
    clipper.AddOpenSubject(openSubject);
    clipper.AddClip(clipPaths);

    Clipper2Lib::Paths64 closedSolution;
    Clipper2Lib::Paths64 openSolution;
    clipper.Execute(Clipper2Lib::ClipType::Intersection,
                    Clipper2Lib::FillRule::NonZero,
                    closedSolution, openSolution);

    // Convert clipped open paths back to Marc::Line segments.
    result.reserve(openSolution.size());
    for (const auto& path : openSolution) {
        for (size_t i = 0; i + 1 < path.size(); ++i) {
            result.emplace_back(
                static_cast<float>(MarcSLM::Core::clipperUnitsToMm(path[i].x)),
                static_cast<float>(MarcSLM::Core::clipperUnitsToMm(path[i].y)),
                static_cast<float>(MarcSLM::Core::clipperUnitsToMm(path[i + 1].x)),
                static_cast<float>(MarcSLM::Core::clipperUnitsToMm(path[i + 1].y)));
        }
    }

    return result;
}

// ==============================================================================
// Endpoint Overlap Extension
// (Ported from Legacy Fill::endpoints_overlap logic)
// ==============================================================================

void HatchGenerator::applyEndpointOverlap(std::vector<Marc::Line>& lines) const {
    if (endpointOverlap_ <= 0.0) return;

    const float overlap = static_cast<float>(endpointOverlap_);

    for (auto& line : lines) {
        // Compute line direction
        float dx = line.b.x - line.a.x;
        float dy = line.b.y - line.a.y;
        float len = std::sqrt(dx * dx + dy * dy);

        if (len < 1e-9f) continue;  // degenerate line

        // Normalize direction
        float nx = dx / len;
        float ny = dy / len;

        // Extend both endpoints along the line direction
        line.a.x -= nx * overlap;
        line.a.y -= ny * overlap;
        line.b.x += nx * overlap;
        line.b.y += ny * overlap;
    }
}

// ==============================================================================
// Rectangle Path Construction (for island cell clipping)
// ==============================================================================

Clipper2Lib::Path64
HatchGenerator::makeRectanglePath(double xMin, double yMin,
                                   double xMax, double yMax) const {
    Clipper2Lib::Path64 rect;
    rect.reserve(4);
    {
        Clipper2Lib::Point64 p;
        p.x = MarcSLM::Core::mmToClipperUnits(xMin);
        p.y = MarcSLM::Core::mmToClipperUnits(yMin);
        rect.push_back(p);
    }
    {
        Clipper2Lib::Point64 p;
        p.x = MarcSLM::Core::mmToClipperUnits(xMax);
        p.y = MarcSLM::Core::mmToClipperUnits(yMin);
        rect.push_back(p);
    }
    {
        Clipper2Lib::Point64 p;
        p.x = MarcSLM::Core::mmToClipperUnits(xMax);
        p.y = MarcSLM::Core::mmToClipperUnits(yMax);
        rect.push_back(p);
    }
    {
        Clipper2Lib::Point64 p;
        p.x = MarcSLM::Core::mmToClipperUnits(xMin);
        p.y = MarcSLM::Core::mmToClipperUnits(yMax);
        rect.push_back(p);
    }
    return rect;
}

// ==============================================================================
// Alternating Scan Direction Sort
// ==============================================================================

void HatchGenerator::sortAlternating(std::vector<Marc::Line>& lines) const {
    if (lines.size() < 2) return;

    // Sort lines by their midpoint's perpendicular distance from the origin.
    // This groups lines by their position in the sweep direction.
    // Then alternate: even lines left-to-right, odd lines right-to-left.

    // First, sort by midpoint Y (or perpendicular coord) for a stable ordering
    std::sort(lines.begin(), lines.end(),
        [](const Marc::Line& a, const Marc::Line& b) {
            float midYa = (a.a.y + a.b.y) * 0.5f;
            float midYb = (b.a.y + b.b.y) * 0.5f;
            return midYa < midYb;
        });

    // Alternate direction: flip every other line
    for (size_t i = 1; i < lines.size(); i += 2) {
        std::swap(lines[i].a, lines[i].b);
    }
}

// ==============================================================================
// Connect Adjacent Hatch Lines into Polylines
// (Ported from Legacy FillRectilinear connection logic)
// ==============================================================================

std::vector<Marc::Polyline>
HatchGenerator::connectHatchLines(const std::vector<Marc::Line>& lines) const {
    std::vector<Marc::Polyline> polylines;

    if (lines.empty()) return polylines;

    // Sort lines by their midpoint perpendicular position
    // to identify adjacent lines that can be connected
    std::vector<const Marc::Line*> sorted;
    sorted.reserve(lines.size());
    for (const auto& l : lines) {
        sorted.push_back(&l);
    }

    // Sort by midpoint Y coordinate (for horizontal sweep) or by the
    // perpendicular distance metric
    std::sort(sorted.begin(), sorted.end(),
        [](const Marc::Line* a, const Marc::Line* b) {
            float midYa = (a->a.y + a->b.y) * 0.5f;
            float midYb = (b->a.y + b->b.y) * 0.5f;
            if (std::abs(midYa - midYb) > 1e-4f)
                return midYa < midYb;
            float midXa = (a->a.x + a->b.x) * 0.5f;
            float midXb = (b->a.x + b->b.x) * 0.5f;
            return midXa < midXb;
        });

    // Build polylines by connecting adjacent lines whose endpoints are close
    // This is a simplified version of the Legacy FillRectilinear connection
    // algorithm, adapted for the Clipper2-based pipeline.
    const float maxGap = static_cast<float>(hatchSpacing_ * 2.5);

    Marc::Polyline currentPoly;

    for (size_t i = 0; i < sorted.size(); ++i) {
        const Marc::Line& line = *sorted[i];

        if (currentPoly.points.empty()) {
            // Start a new polyline
            currentPoly.points.push_back(line.a);
            currentPoly.points.push_back(line.b);
        } else {
            // Check if this line connects to the end of the current polyline
            const Marc::Point& lastPt = currentPoly.points.back();

            float dxA = line.a.x - lastPt.x;
            float dyA = line.a.y - lastPt.y;
            float distA = std::sqrt(dxA * dxA + dyA * dyA);

            float dxB = line.b.x - lastPt.x;
            float dyB = line.b.y - lastPt.y;
            float distB = std::sqrt(dxB * dxB + dyB * dyB);

            if (distA <= maxGap) {
                // Connect: add a travel move to line.a, then line.b
                currentPoly.points.push_back(line.a);
                currentPoly.points.push_back(line.b);
            } else if (distB <= maxGap) {
                // Connect reversed: add travel to line.b, then line.a
                currentPoly.points.push_back(line.b);
                currentPoly.points.push_back(line.a);
            } else {
                // Gap too large — finish current polyline and start new one
                if (currentPoly.isValid()) {
                    polylines.push_back(std::move(currentPoly));
                }
                currentPoly = Marc::Polyline();
                currentPoly.points.push_back(line.a);
                currentPoly.points.push_back(line.b);
            }
        }
    }

    // Flush last polyline
    if (currentPoly.isValid()) {
        polylines.push_back(std::move(currentPoly));
    }

    return polylines;
}

} // namespace PathPlanning
} // namespace MarcSLM
