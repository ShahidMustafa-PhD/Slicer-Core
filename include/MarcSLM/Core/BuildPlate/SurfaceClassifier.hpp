// ==============================================================================
// MarcSLM - Surface Classifier
// ==============================================================================
// Responsibility: Classify each slice surface on a BuildLayer as
//                 Top / Bottom / Internal by comparing the current layer's
//                 polygon coverage against the adjacent upper and lower layers.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// SurfaceClassifier
// ==============================================================================

/// @brief Classifies per-region slice surfaces on a single BuildLayer.
///
/// @details Ported and isolated from the legacy `BuildLayer::detectSurfaceTypes`.
///          For every surface in every region:
///
///   - **Top**:    Any portion not covered by the upper-layer polygons.
///   - **Bottom**: Any portion not covered (supported) by the lower-layer
///                 polygons, provided it was not already classified as Top.
///   - **Internal**: Everything else.
///
///   The classification uses Clipper2 boolean difference operations, which
///   are the same operations performed in the original legacy code.
///
/// ### Thread Safety
///   Stateless: `classify()` operates only on data passed as arguments and
///   is therefore fully re-entrant.  The caller must ensure no concurrent
///   writes to the layer being classified.
class SurfaceClassifier {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    SurfaceClassifier() = default;
    ~SurfaceClassifier() = default;

    SurfaceClassifier(const SurfaceClassifier&)            = default;
    SurfaceClassifier& operator=(const SurfaceClassifier&) = default;
    SurfaceClassifier(SurfaceClassifier&&) noexcept        = default;
    SurfaceClassifier& operator=(SurfaceClassifier&&) noexcept = default;

    // =========================================================================
    // Primary Interface
    // =========================================================================

    /// @brief Classify all surfaces in all regions of a single layer.
    ///
    /// @param layer  The layer whose surfaces are to be classified.
    ///               @c layer.upperLayer and @c layer.lowerLayer are consulted
    ///               but never modified.
    void classify(BuildLayer& layer) const;

    /// @brief Classify surfaces for every layer in a container.
    ///
    /// @param layers  Random-access span of raw layer pointers.
    ///                Null pointers are silently skipped.
    void classifyAll(const std::vector<BuildLayer*>& layers) const;

private:
    // =========================================================================
    // Implementation Helpers
    // =========================================================================

    /// @brief Collect all valid contours from all regions of @p layer into
    ///        a flat Paths64.  Returns an empty Paths64 if layer is null.
    [[nodiscard]] static Clipper2Lib::Paths64 collectContours(
        const BuildLayer* layer);

    /// @brief Classify surfaces in a single BuildLayerRegion.
    static void classifyRegion(BuildLayerRegion&             region,
                                const Clipper2Lib::Paths64&   upperPaths,
                                const Clipper2Lib::Paths64&   lowerPaths);
};

} // namespace BuildPlate
} // namespace MarcSLM
