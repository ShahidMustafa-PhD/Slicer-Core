// ==============================================================================
// MarcSLM - Surface Classifier
// ==============================================================================
// Responsibility: Classify each slice surface on a BuildLayer as
//                 Top / Bottom / Internal by comparing the current layer's
//                 polygon coverage against the adjacent upper and lower layers.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"

#include <vector>

namespace MarcSLM {
namespace BP {

/// @brief Classifies per-region slice surfaces as Top / Bottom / Internal.
///
/// @details Ported and isolated from legacy BuildLayer::detectSurfaceTypes.
///   - Top:      part of the surface not covered by the upper layer.
///   - Bottom:   part not supported by the lower layer (if not already Top).
///   - Internal: everything else.
///
/// Thread safety: stateless — classify() is fully re-entrant.
class SurfaceClassifier {
public:
    SurfaceClassifier() = default;
    ~SurfaceClassifier() = default;
    SurfaceClassifier(const SurfaceClassifier&)            = default;
    SurfaceClassifier& operator=(const SurfaceClassifier&) = default;
    SurfaceClassifier(SurfaceClassifier&&) noexcept        = default;
    SurfaceClassifier& operator=(SurfaceClassifier&&) noexcept = default;

    /// @brief Classify all surfaces in all regions of a single layer.
    ///
    /// @param layer  The layer whose surfaces are to be classified.
    ///               @c layer.upperLayer and @c layer.lowerLayer are consulted
    ///               but never modified.
    void classify(BuildLayer& layer) const;

    /// @brief Classify every layer in the supplied stack. Nulls are skipped.
    ///
    /// @param layers  Random-access span of raw layer pointers.
    ///                Null pointers are silently skipped.
    void classifyAll(const std::vector<BuildLayer*>& layers) const;

private:
    /// @brief Collect all valid contours from every region of @p layer.
    [[nodiscard]] static Clipper2Lib::Paths64 collectContours(
        const BuildLayer* layer);

    /// @brief Classify one BuildLayerRegion using pre-collected neighbour paths.
    static void classifyRegion(BuildLayerRegion&             region,
                                const Clipper2Lib::Paths64&   upperPaths,
                                const Clipper2Lib::Paths64&   lowerPaths);
};

} // namespace BP
} // namespace MarcSLM
