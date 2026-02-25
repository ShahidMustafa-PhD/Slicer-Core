// ==============================================================================
// MarcSLM - Contour / Hatch Splitter
// ==============================================================================
// Stage 4 of PySLM pipeline: Within each region, separate the thin outer
// boundary ring (Contour) from the interior (Hatch) via inward offset.
//
// PySLM logic:
//   hatchRegion   = region.buffer(-contourOffset)
//   contourRegion = region.difference(hatchRegion)
//
// An optional ContourHatch transition strip is computed as the thin region
// where the contour ring meets the hatch interior (Konturversatz).
//
// Thread-safe: all methods are const with no shared mutable state.
// ==============================================================================

#pragma once

#include "MarcSLM/Thermal/ClipperBoolean.hpp"

#include <clipper2/clipper.h>

namespace MarcSLM {
namespace Thermal {

/// @brief Result of contour/hatch separation for a single region.
struct ContourHatchSplit {
    Clipper2Lib::Paths64 contour;      ///< Thin outer boundary ring
    Clipper2Lib::Paths64 hatch;        ///< Interior area for infill hatching
    Clipper2Lib::Paths64 contourHatch; ///< Transition strip (Konturversatz)
};

/// @brief Splits a region into Contour ring, Hatch interior, and
///        an optional ContourHatch transition strip.
class ContourHatchSplitter {
public:
    /// @param contourDelta  Contour ring width in Clipper2 integer units.
    /// @param contourHatchFraction  Fraction of contourDelta used for the
    ///                              transition strip (0..1, typically 0.3).
    /// @param miterLimit  Miter limit for corner preservation.
    ContourHatchSplitter(double contourDelta,
                         double contourHatchFraction = 0.3,
                         double miterLimit = 3.0) noexcept
        : contourDelta_(contourDelta)
        , chFraction_(contourHatchFraction)
        , miterLimit_(miterLimit) {}

    /// @brief Split a region polygon set into contour, hatch, and contourHatch.
    [[nodiscard]] ContourHatchSplit
    split(const Clipper2Lib::Paths64& region) const noexcept
    {
        ContourHatchSplit result;

        if (region.empty()) return result;

        // Hatch = OffsetInward(region, contourWidth)
        result.hatch = ClipperBoolean::offsetInward(
            region, contourDelta_, miterLimit_);

        // Contour = region - hatch (the thin outer ring)
        result.contour = ClipperBoolean::difference(region, result.hatch);

        // ContourHatch = transition strip at the contour/hatch boundary
        // Computed as: Intersect(contour, OffsetOutward(hatch, smallOverlap))
        if (!result.contour.empty() && !result.hatch.empty()) {
            const double chDelta = contourDelta_ * chFraction_;
            auto grownHatch = ClipperBoolean::offsetOutward(
                result.hatch, chDelta, miterLimit_);
            result.contourHatch = ClipperBoolean::intersect(
                result.contour, grownHatch);
        }

        return result;
    }

private:
    double contourDelta_;
    double chFraction_;
    double miterLimit_;
};

} // namespace Thermal
} // namespace MarcSLM
