// ==============================================================================
// MarcSLM - Clipper2 Boolean Operation Wrappers
// ==============================================================================
// Stateless, header-only thin wrappers around Clipper2Lib for SLM segmentation.
//
// Design rationale:
//   - All functions are static and thread-safe (no mutable state).
//   - FillRule::NonZero is used throughout to handle self-intersecting
//     polygons common in raw STL slices.
//   - JoinType::Miter preserves sharp mechanical corners during offsetting.
//   - All distances are in Clipper2 integer units (pre-scaled by caller).
// ==============================================================================

#pragma once

#include <clipper2/clipper.h>

namespace MarcSLM {
namespace Thermal {

/// @brief Thread-safe, stateless Clipper2 Boolean operation wrappers.
struct ClipperBoolean {

    /// @brief Compute the intersection (AND) of two polygon sets.
    [[nodiscard]] static Clipper2Lib::Paths64
    intersect(const Clipper2Lib::Paths64& subject,
              const Clipper2Lib::Paths64& clip) noexcept
    {
        if (subject.empty() || clip.empty()) return {};
        return Clipper2Lib::Intersect(subject, clip,
                                      Clipper2Lib::FillRule::NonZero);
    }

    /// @brief Compute the difference (MINUS) of two polygon sets.
    [[nodiscard]] static Clipper2Lib::Paths64
    difference(const Clipper2Lib::Paths64& subject,
               const Clipper2Lib::Paths64& clip) noexcept
    {
        if (subject.empty()) return {};
        if (clip.empty())    return subject;
        return Clipper2Lib::Difference(subject, clip,
                                       Clipper2Lib::FillRule::NonZero);
    }

    /// @brief Compute the union (OR) of two polygon sets.
    [[nodiscard]] static Clipper2Lib::Paths64
    unite(const Clipper2Lib::Paths64& subject,
          const Clipper2Lib::Paths64& clip) noexcept
    {
        return Clipper2Lib::Union(subject, clip,
                                  Clipper2Lib::FillRule::NonZero);
    }

    /// @brief Compute the union of a single polygon set (self-union / cleanup).
    [[nodiscard]] static Clipper2Lib::Paths64
    unite(const Clipper2Lib::Paths64& paths) noexcept
    {
        if (paths.empty()) return {};
        return Clipper2Lib::Union(paths, Clipper2Lib::FillRule::NonZero);
    }

    /// @brief Inward (negative) polygon offset.
    /// @param paths   Input polygon set.
    /// @param delta   Offset distance in Clipper2 integer units (positive value;
    ///                the function applies the negation internally).
    /// @param miterLimit  Miter limit for sharp-corner preservation.
    /// @return Deflated polygon set.  Empty if the input collapses.
    [[nodiscard]] static Clipper2Lib::Paths64
    offsetInward(const Clipper2Lib::Paths64& paths,
                 double delta,
                 double miterLimit = 3.0) noexcept
    {
        if (paths.empty() || delta <= 0.0) return paths;
        return Clipper2Lib::InflatePaths(
            paths, -delta,
            Clipper2Lib::JoinType::Miter,
            Clipper2Lib::EndType::Polygon,
            miterLimit);
    }

    /// @brief Outward (positive) polygon offset.
    [[nodiscard]] static Clipper2Lib::Paths64
    offsetOutward(const Clipper2Lib::Paths64& paths,
                  double delta,
                  double miterLimit = 3.0) noexcept
    {
        if (paths.empty() || delta <= 0.0) return paths;
        return Clipper2Lib::InflatePaths(
            paths, delta,
            Clipper2Lib::JoinType::Miter,
            Clipper2Lib::EndType::Polygon,
            miterLimit);
    }
};

} // namespace Thermal
} // namespace MarcSLM
