// ==============================================================================
// MarcSLM - Surface Classifier — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/SurfaceClassifier.hpp"

#include <clipper2/clipper.h>

namespace MarcSLM {
namespace BP {

// ==============================================================================
// Public Interface
// ==============================================================================

void SurfaceClassifier::classify(BuildLayer& layer) const
{
    const Clipper2Lib::Paths64 upperPaths = collectContours(layer.upperLayer);
    const Clipper2Lib::Paths64 lowerPaths = collectContours(layer.lowerLayer);

    for (auto* regionPtr : layer.regions()) {
        if (regionPtr) classifyRegion(*regionPtr, upperPaths, lowerPaths);
    }
}

void SurfaceClassifier::classifyAll(const std::vector<BuildLayer*>& layers) const
{
    for (auto* layer : layers) {
        if (layer) classify(*layer);
    }
}

// ==============================================================================
// Private Helpers
// ==============================================================================

Clipper2Lib::Paths64 SurfaceClassifier::collectContours(const BuildLayer* layer)
{
    Clipper2Lib::Paths64 paths;
    if (!layer) return paths;
    for (const auto* region : layer->regions()) {
        if (!region) continue;
        for (const auto& surf : region->slices) {
            if (surf.isValid()) paths.push_back(surf.contour);
        }
    }
    return paths;
}

void SurfaceClassifier::classifyRegion(
    BuildLayerRegion&           region,
    const Clipper2Lib::Paths64& upperPaths,
    const Clipper2Lib::Paths64& lowerPaths)
{
    for (auto& surf : region.slices) {
        if (!surf.isValid()) continue;

        surf.type = SurfaceType::Internal;  // default

        const Clipper2Lib::Paths64 current = {surf.contour};

        // --- Top test ---
        bool isTop = upperPaths.empty();
        if (!isTop) {
            const Clipper2Lib::Paths64 exposed =
                Clipper2Lib::BooleanOp(Clipper2Lib::ClipType::Difference,
                                       Clipper2Lib::FillRule::NonZero,
                                       current, upperPaths);
            isTop = !exposed.empty();
        }
        if (isTop) { surf.type = SurfaceType::Top; continue; }

        // --- Bottom test ---
        bool isBottom = lowerPaths.empty();
        if (!isBottom) {
            const Clipper2Lib::Paths64 unsupported =
                Clipper2Lib::BooleanOp(Clipper2Lib::ClipType::Difference,
                                       Clipper2Lib::FillRule::NonZero,
                                       current, lowerPaths);
            isBottom = !unsupported.empty();
        }
        if (isBottom) surf.type = SurfaceType::Bottom;
        // else: remains Internal
    }
}

} // namespace BP
} // namespace MarcSLM
