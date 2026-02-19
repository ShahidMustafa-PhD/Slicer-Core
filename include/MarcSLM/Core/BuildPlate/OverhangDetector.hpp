// ==============================================================================
// MarcSLM - Overhang Detector
// ==============================================================================
// Responsibility: For each layer, compute the regions of the current layer
//                 that exceed the critical overhang angle and therefore require
//                 support material.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"

#include <cstddef>
#include <map>

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// OverhangResult
// ==============================================================================

/// @brief Overhang geometry per region per layer.
///
/// Key: region index.
/// Value: map of (layer index ? unsupported Paths64).
using OverhangMap = std::map<std::size_t,
                             std::map<std::size_t, Clipper2Lib::Paths64>>;

// ==============================================================================
// OverhangDetector
// ==============================================================================

/// @brief Detects unsupported (overhang) areas for every layer in a build object.
///
/// @details Ported and isolated from the legacy
///          `PrintObject::detectOverhangs` method.
///
///          For each layer L (starting at layer 1), for each region R:
///            1. Expand the lower layer's footprint by the critical lateral
///               distance = layer_height / tan(support_angle).
///            2. Subtract the expanded lower footprint from the current layer.
///            3. Any remaining area is an unsupported overhang.
///
/// ### Thread Safety
///   Stateless: `detect()` is fully re-entrant provided the caller does not
///   concurrently write to the layers being examined.
class OverhangDetector {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    OverhangDetector() = default;
    ~OverhangDetector() = default;

    OverhangDetector(const OverhangDetector&)            = default;
    OverhangDetector& operator=(const OverhangDetector&) = default;
    OverhangDetector(OverhangDetector&&) noexcept        = default;
    OverhangDetector& operator=(OverhangDetector&&) noexcept = default;

    // =========================================================================
    // Primary Interface
    // =========================================================================

    /// @brief Scan all layers and return the overhang areas map.
    ///
    /// @param layers        Ordered layer pointers from bottom to top.
    ///                      Layer 0 is not examined (has no lower neighbour).
    /// @param supportAngle  Critical overhang angle [degrees].  Regions that
    ///                      project beyond this angle are flagged.
    ///
    /// @return OverhangMap keyed by (regionIdx ? (layerIdx ? unsupported paths)).
    [[nodiscard]] OverhangMap detect(
        const std::vector<BuildLayer*>& layers,
        double                          supportAngle) const;

private:
    // =========================================================================
    // Implementation Helpers
    // =========================================================================

    /// @brief Compute the critical lateral offset [Clipper2 integer units]
    ///        at which a surface becomes an overhang.
    ///
    /// @param layerHeight   Height of the current layer [mm].
    /// @param supportAngle  Overhang angle threshold [degrees].
    /// @return              Offset in Clipper2 integer coordinate units.
    [[nodiscard]] static int64_t computeOffset(double layerHeight,
                                               double supportAngle) noexcept;

    /// @brief Collect all valid contours from a single BuildLayerRegion.
    [[nodiscard]] static Clipper2Lib::Paths64 regionContours(
        const BuildLayerRegion& region);
};

} // namespace BuildPlate
} // namespace MarcSLM
