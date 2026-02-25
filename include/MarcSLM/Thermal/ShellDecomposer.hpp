// ==============================================================================
// MarcSLM - Shell Decomposer
// ==============================================================================
// Stage 3 of PySLM pipeline: Shell1 / Shell2 / Core decomposition.
//
// PySLM logic (consecutive inward offsets):
//   inner1 = layerArea.buffer(-shell1_offset)
//   shell1 = layerArea.difference(inner1)
//   inner2 = inner1.buffer(-shell2_offset)
//   shell2 = inner1.difference(inner2)
//   core   = inner2
//
// Thread-safe: all methods are const with no shared mutable state.
// ==============================================================================

#pragma once

#include "MarcSLM/Thermal/ClipperBoolean.hpp"

#include <clipper2/clipper.h>

namespace MarcSLM {
namespace Thermal {

/// @brief Result of shell/core decomposition for a single layer.
struct ShellDecomposition {
    Clipper2Lib::Paths64 shell1;  ///< Outermost shell ring
    Clipper2Lib::Paths64 shell2;  ///< Second shell ring (inside shell1)
    Clipper2Lib::Paths64 core;    ///< Remaining interior after both shells
};

/// @brief Decomposes a layer polygon into Shell1, Shell2, and Core regions.
/// @details Uses consecutive inward offsets with Miter join to preserve
///          sharp mechanical corners (critical for SLM parts).
///
///          If a shell offset collapses the geometry entirely (part is thinner
///          than the shell thickness), the corresponding shell region will be
///          empty and the remaining geometry stays in the outer region.
class ShellDecomposer {
public:
    /// @param shell1Delta  Shell 1 thickness in Clipper2 integer units.
    /// @param shell2Delta  Shell 2 thickness in Clipper2 integer units.
    /// @param miterLimit   Miter limit for corner preservation.
    ShellDecomposer(double shell1Delta,
                    double shell2Delta,
                    double miterLimit = 3.0) noexcept
        : shell1Delta_(shell1Delta)
        , shell2Delta_(shell2Delta)
        , miterLimit_(miterLimit) {}

    /// @brief Decompose the layer polygon set into three concentric regions.
    /// @param layerArea  The full solid-area polygon set for this layer.
    [[nodiscard]] ShellDecomposition
    decompose(const Clipper2Lib::Paths64& layerArea) const noexcept
    {
        ShellDecomposition result;

        if (layerArea.empty()) return result;

        // Step 1: Deflate by shell1 thickness -> inner1
        auto inner1 = ClipperBoolean::offsetInward(
            layerArea, shell1Delta_, miterLimit_);

        // Shell1 = layerArea - inner1 (the outermost ring)
        result.shell1 = ClipperBoolean::difference(layerArea, inner1);

        if (inner1.empty()) {
            // Part is thinner than shell1: no shell2 or core
            return result;
        }

        // Step 2: Deflate inner1 by shell2 thickness -> inner2
        auto inner2 = ClipperBoolean::offsetInward(
            inner1, shell2Delta_, miterLimit_);

        // Shell2 = inner1 - inner2 (the second ring)
        result.shell2 = ClipperBoolean::difference(inner1, inner2);

        // Core = inner2 (everything remaining inside both shells)
        result.core = std::move(inner2);

        return result;
    }

private:
    double shell1Delta_;
    double shell2Delta_;
    double miterLimit_;
};

} // namespace Thermal
} // namespace MarcSLM
