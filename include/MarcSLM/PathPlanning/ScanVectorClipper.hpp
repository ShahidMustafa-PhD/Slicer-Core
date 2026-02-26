// ==============================================================================
// MarcSLM - Scan Vector Clipper
// ==============================================================================
// Robust two-step clipping of open scan vector paths against ExPolygons
// (contour + holes) using Clipper2.
//
// Algorithm:
//   Step 1: Intersect open scan lines with the outer contour
//           → segments are bounded to the contour perimeter.
//   Step 2: Difference the clipped segments with hole polygons
//           → segments passing through voids are removed.
//
// Design:
//   - Stateless, thread-safe (all static methods).
//   - Follows the same pattern as ClipperBoolean.
//   - Separation of concerns: clipping logic is isolated from hatch generation.
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/Types.hpp"

#include <clipper2/clipper.h>

#include <vector>

namespace MarcSLM {
namespace PathPlanning {

/// @brief Stateless utility for clipping open scan vectors against ExPolygons.
/// @details Implements a two-step Clipper2 algorithm that guarantees scan
///          vectors are bounded by the contour and excluded from all holes.
///          Thread-safe — all methods are static with no mutable state.
class ScanVectorClipper {
public:
  ScanVectorClipper() = delete;

  // ==========================================================================
  // Public API
  // ==========================================================================

  /// @brief Clip scan lines to the solid region of an ExPolygon.
  /// @details Two-step algorithm:
  ///   1. Intersect lines with contour → bounded to outer perimeter
  ///   2. Difference result with holes → excluded from voids
  ///
  /// @param lines     Scan line segments in mm coordinates.
  /// @param contour   Outer boundary (Clipper2 Path64, CCW winding).
  ///                  Must be in Clipper2 integer units (pre-scaled).
  /// @param holes     Inner hole polygons (Clipper2 Paths64, CW winding).
  ///                  Must be in Clipper2 integer units (pre-scaled).
  /// @return Clipped line segments in mm coordinates. Empty if no segments
  ///         survive clipping.
  [[nodiscard]] static std::vector<Marc::Line>
  clipToExPolygon(const std::vector<Marc::Line> &lines,
                  const Clipper2Lib::Path64 &contour,
                  const Clipper2Lib::Paths64 &holes);

private:
  // ==========================================================================
  // Internal Operations
  // ==========================================================================

  /// @brief Step 1: Intersect open paths with a single closed polygon.
  /// @param openPaths  Open scan line paths in Clipper2 integer units.
  /// @param polygon    Closed polygon boundary in Clipper2 integer units.
  /// @return Clipped open paths that lie within the polygon.
  [[nodiscard]] static Clipper2Lib::Paths64
  intersectOpenWithPolygon(const Clipper2Lib::Paths64 &openPaths,
                           const Clipper2Lib::Path64 &polygon);

  /// @brief Step 2: Subtract hole polygons from open paths.
  /// @param openPaths  Open scan line paths in Clipper2 integer units.
  /// @param holes      Hole polygons in Clipper2 integer units.
  /// @return Open paths with hole regions removed. A single input path
  ///         may produce multiple output segments if it crosses a hole.
  [[nodiscard]] static Clipper2Lib::Paths64
  differenceOpenWithPolygons(const Clipper2Lib::Paths64 &openPaths,
                             const Clipper2Lib::Paths64 &holes);

  // ==========================================================================
  // Coordinate Conversion Helpers
  // ==========================================================================

  /// @brief Convert Marc::Line vector to Clipper2 open paths (mm → integer
  /// units).
  [[nodiscard]] static Clipper2Lib::Paths64
  linesToOpenPaths(const std::vector<Marc::Line> &lines);

  /// @brief Convert Clipper2 open paths to Marc::Line vector (integer units →
  /// mm).
  [[nodiscard]] static std::vector<Marc::Line>
  openPathsToLines(const Clipper2Lib::Paths64 &paths);
};

} // namespace PathPlanning
} // namespace MarcSLM
