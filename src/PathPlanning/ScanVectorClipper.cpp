// ==============================================================================
// MarcSLM - Scan Vector Clipper Implementation
// ==============================================================================
// Two-step Clipper2 clipping: intersect with contour, then difference with
// holes.
// ==============================================================================

#include "MarcSLM/PathPlanning/ScanVectorClipper.hpp"
#include "MarcSLM/Core/Types.hpp"

namespace MarcSLM {
namespace PathPlanning {

// ==============================================================================
// Public API: clipToExPolygon
// ==============================================================================

std::vector<Marc::Line>
ScanVectorClipper::clipToExPolygon(const std::vector<Marc::Line> &lines,
                                   const Clipper2Lib::Path64 &contour,
                                   const Clipper2Lib::Paths64 &holes) {
  if (lines.empty() || contour.size() < 3) {
    return {};
  }

  // Convert input lines (mm) to Clipper2 open paths (integer units).
  Clipper2Lib::Paths64 openPaths = linesToOpenPaths(lines);

  // ---- Step 1: Intersect with contour ----
  // Produces only the segments that lie within the outer boundary.
  Clipper2Lib::Paths64 contourClipped =
      intersectOpenWithPolygon(openPaths, contour);

  if (contourClipped.empty()) {
    return {};
  }

  // ---- Step 2: Subtract holes ----
  // Remove any segments (or portions thereof) that pass through voids.
  // Skip this step if there are no holes — minor optimisation.
  Clipper2Lib::Paths64 finalPaths;
  if (!holes.empty()) {
    // Filter to only valid hole polygons (>= 3 vertices).
    Clipper2Lib::Paths64 validHoles;
    validHoles.reserve(holes.size());
    for (const auto &hole : holes) {
      if (hole.size() >= 3) {
        validHoles.push_back(hole);
      }
    }

    if (!validHoles.empty()) {
      finalPaths = differenceOpenWithPolygons(contourClipped, validHoles);
    } else {
      finalPaths = std::move(contourClipped);
    }
  } else {
    finalPaths = std::move(contourClipped);
  }

  // Convert Clipper2 open paths (integer units) back to Marc::Line (mm).
  return openPathsToLines(finalPaths);
}

// ==============================================================================
// Step 1: Intersect open paths with a single closed polygon
// ==============================================================================

Clipper2Lib::Paths64 ScanVectorClipper::intersectOpenWithPolygon(
    const Clipper2Lib::Paths64 &openPaths, const Clipper2Lib::Path64 &polygon) {

  Clipper2Lib::Clipper64 clipper;
  clipper.AddOpenSubject(openPaths);
  clipper.AddClip({polygon});

  Clipper2Lib::Paths64 closedSolution;
  Clipper2Lib::Paths64 openSolution;
  clipper.Execute(Clipper2Lib::ClipType::Intersection,
                  Clipper2Lib::FillRule::NonZero, closedSolution, openSolution);

  return openSolution;
}

// ==============================================================================
// Step 2: Subtract hole polygons from open paths
// ==============================================================================

Clipper2Lib::Paths64 ScanVectorClipper::differenceOpenWithPolygons(
    const Clipper2Lib::Paths64 &openPaths, const Clipper2Lib::Paths64 &holes) {

  Clipper2Lib::Clipper64 clipper;
  clipper.AddOpenSubject(openPaths);
  clipper.AddClip(holes);

  Clipper2Lib::Paths64 closedSolution;
  Clipper2Lib::Paths64 openSolution;
  clipper.Execute(Clipper2Lib::ClipType::Difference,
                  Clipper2Lib::FillRule::NonZero, closedSolution, openSolution);

  return openSolution;
}

// ==============================================================================
// Coordinate Conversion: Marc::Line → Clipper2 open paths
// ==============================================================================

Clipper2Lib::Paths64
ScanVectorClipper::linesToOpenPaths(const std::vector<Marc::Line> &lines) {
  Clipper2Lib::Paths64 openPaths;
  openPaths.reserve(lines.size());

  for (const auto &line : lines) {
    Clipper2Lib::Path64 path;
    path.reserve(2);
    path.emplace_back(Core::mmToClipperUnits(static_cast<double>(line.a.x)),
                      Core::mmToClipperUnits(static_cast<double>(line.a.y)));
    path.emplace_back(Core::mmToClipperUnits(static_cast<double>(line.b.x)),
                      Core::mmToClipperUnits(static_cast<double>(line.b.y)));
    openPaths.push_back(std::move(path));
  }

  return openPaths;
}

// ==============================================================================
// Coordinate Conversion: Clipper2 open paths → Marc::Line
// ==============================================================================

std::vector<Marc::Line>
ScanVectorClipper::openPathsToLines(const Clipper2Lib::Paths64 &paths) {
  std::vector<Marc::Line> result;
  result.reserve(paths.size());

  for (const auto &path : paths) {
    // Each open path may contain 2+ points. Extract consecutive segments.
    for (size_t i = 0; i + 1 < path.size(); ++i) {
      result.emplace_back(
          static_cast<float>(Core::clipperUnitsToMm(path[i].x)),
          static_cast<float>(Core::clipperUnitsToMm(path[i].y)),
          static_cast<float>(Core::clipperUnitsToMm(path[i + 1].x)),
          static_cast<float>(Core::clipperUnitsToMm(path[i + 1].y)));
    }
  }

  return result;
}

} // namespace PathPlanning
} // namespace MarcSLM
