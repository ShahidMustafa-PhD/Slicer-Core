// ==============================================================================
// MarcSLM - BuildTypes — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"

#include <clipper2/clipper.h>

namespace MarcSLM {

// ==============================================================================
// BuildLayerRegion
// ==============================================================================

BuildLayerRegion::BuildLayerRegion(BuildLayer*  layer,
                                    PrintRegion* region) noexcept
    : layer_(layer), region_(region)
{}

void BuildLayerRegion::prepareFillSurfaces()
{
    if (fillSurfaces.empty() && !slices.empty())
        fillSurfaces = slices;
}

// ==============================================================================
// BuildLayer
// ==============================================================================

BuildLayer::BuildLayer(std::size_t  id, PrintObject* object,
                        double height, double printZ, double sliceZ)
    : id_(id), object_(object), height_(height), printZ_(printZ), sliceZ_(sliceZ)
{}

BuildLayer::~BuildLayer()
{
    for (auto* r : regions_) delete r;
    regions_.clear();
}

BuildLayerRegion* BuildLayer::addRegion(PrintRegion* region)
{
    auto* lr = new BuildLayerRegion(this, region);
    regions_.push_back(lr);
    return lr;
}

BuildLayerRegion* BuildLayer::getRegion(std::size_t idx) noexcept
{
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

const BuildLayerRegion* BuildLayer::getRegion(std::size_t idx) const noexcept
{
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

void BuildLayer::makeSlices()
{
    mergedSlices.clear();

    Clipper2Lib::Paths64 allPaths;
    allPaths.reserve(regions_.size() * 4);

    for (const auto* lr : regions_) {
        if (!lr) continue;
        for (const auto& surf : lr->slices) {
            if (surf.isValid()) allPaths.push_back(surf.contour);
        }
    }
    if (allPaths.empty()) return;

    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(allPaths);
    clipper.Execute(Clipper2Lib::ClipType::Union,
                    Clipper2Lib::FillRule::NonZero,
                    mergedSlices);
}

} // namespace MarcSLM
