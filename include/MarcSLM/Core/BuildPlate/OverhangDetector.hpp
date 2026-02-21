// ==============================================================================
// MarcSLM - Overhang Detector
// ==============================================================================
#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace MarcSLM {
namespace BP {

/// @brief Overhang geometry per region per layer.
/// Key: region index  ?  (layer index  ?  unsupported Paths64).
using OverhangMap = std::map<std::size_t,
                             std::map<std::size_t, Clipper2Lib::Paths64>>;

/// @brief Detects unsupported overhang areas for every layer in a build object.
///
/// @details Ported and isolated from legacy PrintObject::detectOverhangs.
///          For each layer L (starting at 1), for each region R:
///            1. Expand lower layer footprint by critical lateral distance
///               = layer_height / tan(support_angle).
///            2. Current – expanded_lower  = unsupported area.
///
/// Thread safety: stateless — detect() is fully re-entrant.
class OverhangDetector {
public:
    OverhangDetector() = default;
    ~OverhangDetector() = default;
    OverhangDetector(const OverhangDetector&)            = default;
    OverhangDetector& operator=(const OverhangDetector&) = default;
    OverhangDetector(OverhangDetector&&) noexcept        = default;
    OverhangDetector& operator=(OverhangDetector&&) noexcept = default;

    /// @brief Scan all layers and return the overhang area map.
    ///
    /// @param layers        Ordered layer pointers (bottom ? top). Layer 0 skipped.
    /// @param supportAngle  Critical overhang angle [degrees].
    [[nodiscard]] OverhangMap detect(
        const std::vector<BuildLayer*>& layers,
        double                          supportAngle) const;

private:
    /// @brief Clipper2 integer offset at which a surface becomes an overhang.
    [[nodiscard]] static int64_t computeOffset(double layerHeight,
                                               double supportAngle) noexcept;

    /// @brief Collect all valid contours from a single BuildLayerRegion.
    [[nodiscard]] static Clipper2Lib::Paths64 regionContours(
        const BuildLayerRegion& region);
};

} // namespace BP
} // namespace MarcSLM
