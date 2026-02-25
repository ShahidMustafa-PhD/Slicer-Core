// ==============================================================================
// MarcSLM - Thermal Mask Generator
// ==============================================================================
// Stage 2 of PySLM pipeline: Volume / Overhang (Downskin) mask computation.
//
// PySLM logic:
//   volumeRegion   = currentLayer.intersection(prevLayer)
//   overhangRegion = currentLayer.difference(prevLayer)
//   First layer    = 100% overhang (no previous layer support)
//
// Thread-safe: all methods are const with no shared mutable state.
// ==============================================================================

#pragma once

#include "MarcSLM/Thermal/ClipperBoolean.hpp"

#include <clipper2/clipper.h>

namespace MarcSLM {
namespace Thermal {

/// @brief Result of thermal mask computation for a single layer.
struct ThermalMasks {
    Clipper2Lib::Paths64 volume;    ///< Regions supported by previous layer (inskin)
    Clipper2Lib::Paths64 overhang;  ///< Regions over powder / unsupported (downskin)
};

/// @brief Computes volume and overhang masks by comparing consecutive layers.
/// @details Implements PySLM's downskin detection:
///   - Volume  = Intersection(L_n, L_{n-1})
///   - Overhang = Difference(L_n, L_{n-1})
///
/// For the first layer (no previous), the entire area is classified as overhang.
/// This is physically correct: the first layer sits directly on the powder bed
/// (or build plate), which requires overhang-specific laser parameters.
class ThermalMaskGenerator {
public:
    /// @brief Compute thermal masks for a layer.
    /// @param current   Solid-area polygon set of the current layer L_n.
    /// @param previous  Solid-area polygon set of the previous layer L_{n-1}.
    ///                  Pass nullptr for the first layer.
    /// @return Volume and overhang masks.
    [[nodiscard]] static ThermalMasks
    compute(const Clipper2Lib::Paths64& current,
            const Clipper2Lib::Paths64* previous) noexcept
    {
        ThermalMasks masks;

        if (previous != nullptr && !previous->empty()) {
            // Volume = areas of L_n that overlap with L_{n-1} (supported)
            masks.volume   = ClipperBoolean::intersect(current, *previous);
            // Overhang = areas of L_n NOT in L_{n-1} (unsupported / over powder)
            masks.overhang = ClipperBoolean::difference(current, *previous);
        } else {
            // First layer: entire area is overhang (on powder bed)
            masks.overhang = current;
            // masks.volume remains empty
        }

        return masks;
    }
};

} // namespace Thermal
} // namespace MarcSLM
