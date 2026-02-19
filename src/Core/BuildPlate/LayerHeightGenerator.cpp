// ==============================================================================
// MarcSLM - Layer Height Generator — Implementation
// ==============================================================================

#include "MarcSLM/Core/BuildPlate/LayerHeightGenerator.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// Public Interface
// ==============================================================================

std::vector<double> LayerHeightGenerator::generate(
    const SlmConfig& config,
    double           objectHeight,
    double           firstLayerHeightOverride) const
{
    if (objectHeight <= 0.0) return {};

    // -----------------------------------------------------------------
    // Resolve nominal layer height, quantised to machine z-resolution
    // -----------------------------------------------------------------
    double nominalH = config.layer_thickness;
    const double minDz = (config.z_steps_per_mm > 0.0)
                       ? (1.0 / config.z_steps_per_mm)
                       : 0.0;

    if (minDz > 0.0) {
        nominalH = quantise(nominalH, minDz);
        if (nominalH <= 0.0) nominalH = config.layer_thickness;
    }

    // -----------------------------------------------------------------
    // Resolve first-layer height
    // -----------------------------------------------------------------
    double firstH = (firstLayerHeightOverride > 0.0)
                  ? firstLayerHeightOverride
                  : config.first_layer_thickness;

    if (firstH <= 0.0) firstH = nominalH;
    if (minDz > 0.0)   firstH = quantise(firstH, minDz);

    // -----------------------------------------------------------------
    // Build the height vector
    // -----------------------------------------------------------------
    std::vector<double> result;
    result.reserve(static_cast<std::size_t>(objectHeight / nominalH) + 4);

    result.push_back(firstH);
    double printZ = firstH;

    while ((printZ + 1e-9) < objectHeight) {
        printZ += nominalH;
        result.push_back(printZ);
    }

    // -----------------------------------------------------------------
    // Adjust last layer to match object height exactly
    // -----------------------------------------------------------------
    if (result.size() > 1) {
        adjustLastLayer(result, objectHeight, nominalH);
    }

    // -----------------------------------------------------------------
    // Apply z-gradation rounding
    // -----------------------------------------------------------------
    if (minDz > 0.0) {
        applyGradation(result, minDz);
    }

    return result;
}

// ==============================================================================
// Private Helpers
// ==============================================================================

double LayerHeightGenerator::quantise(double height, double minDz) noexcept
{
    assert(minDz > 0.0);
    const double rounded = std::round(height / minDz) * minDz;
    return (rounded > 0.0) ? rounded : minDz;
}

void LayerHeightGenerator::applyGradation(std::vector<double>& heights,
                                           double               gradation) noexcept
{
    assert(gradation > 0.0);
    double lastZ = 0.0;
    for (double& z : heights) {
        double h         = z - lastZ;
        double remainder = std::fmod(h, gradation);
        if (remainder > gradation * 0.5) {
            h += (gradation - remainder);
        } else {
            h -= remainder;
        }
        // Never allow a zero-height layer
        if (h < gradation) h = gradation;
        z     = lastZ + h;
        lastZ = z;
    }
}

void LayerHeightGenerator::adjustLastLayer(std::vector<double>& heights,
                                            double               objectHeight,
                                            double               nominalH) noexcept
{
    if (heights.empty()) return;

    const std::size_t last  = heights.size() - 1;
    const double      prevZ = (last > 0) ? heights[last - 1] : 0.0;
    const double      diff  = heights[last] - objectHeight;
    double            newH  = heights[last] - prevZ;

    if (diff < 0.0) {
        // Stack falls short — thicken last layer
        newH = std::min(nominalH * 1.5, newH - diff);
    } else {
        // Stack overshoots — thin last layer
        newH = std::max(nominalH * 0.5, newH - diff);
    }

    heights[last] = prevZ + newH;
}

} // namespace BuildPlate
} // namespace MarcSLM
