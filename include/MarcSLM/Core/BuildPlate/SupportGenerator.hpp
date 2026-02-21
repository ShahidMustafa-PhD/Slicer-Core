// ==============================================================================
// MarcSLM - Support Generator
// ==============================================================================
#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"
#include "MarcSLM/Core/BuildPlate/OverhangDetector.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"

#include <cstddef>
#include <map>
#include <vector>

namespace MarcSLM {
namespace BP {

// ==============================================================================
// SupportPillar — value type
// ==============================================================================

/// @brief Descriptor for a single tapered support pillar.
struct SupportPillar {
    double      x           = 0.0;
    double      y           = 0.0;
    double      radiusBase  = 0.0;
    double      radiusTop   = 0.0;
    std::size_t topLayer    = 0;
    std::size_t bottomLayer = 0;
};

// ==============================================================================
// SupportGenerator
// ==============================================================================

/// @brief Generates tapered pillar support structures for a set of layers.
///
/// @details Three-stage pipeline (ported from legacy PrintObject support methods):
///   1. Pillar placement — grid-sample union of all overhang areas.
///   2. Vertical extent  — walk layers top?bottom per XY position.
///   3. Geometry stamp   — write tapered-circle surfaces into layer-regions.
///
/// Thread safety: stateless — generate() is fully re-entrant.
class SupportGenerator {
public:
    SupportGenerator() = default;
    ~SupportGenerator() = default;
    SupportGenerator(const SupportGenerator&)            = default;
    SupportGenerator& operator=(const SupportGenerator&) = default;
    SupportGenerator(SupportGenerator&&) noexcept        = default;
    SupportGenerator& operator=(SupportGenerator&&) noexcept = default;

    /// @brief Build all support pillars and stamp geometry into the layer stack.
    ///
    /// @param layers    Ordered layer stack (bottom ? top) to be modified.
    /// @param overhangs Overhang map from OverhangDetector::detect().
    /// @param config    SLM configuration for pillar sizing and spacing.
    void generate(const std::vector<BuildLayer*>& layers,
                  const OverhangMap&              overhangs,
                  const SlmConfig&                config) const;

private:
    // --- Stage 1 ---
    [[nodiscard]] static std::map<std::size_t,
                                  std::vector<Clipper2Lib::Point64>>
    placePillars(const OverhangMap& overhangs, double pillarSpacing);

    // --- Stage 2 ---
    [[nodiscard]] static std::map<std::size_t, std::vector<SupportPillar>>
    resolveExtents(
        const std::vector<BuildLayer*>&                            layers,
        const std::map<std::size_t,
                       std::vector<Clipper2Lib::Point64>>& positions,
        double                                              pillarSize);

    // --- Stage 3 ---
    static void stampGeometry(
        const std::vector<BuildLayer*>&                           layers,
        const std::map<std::size_t, std::vector<SupportPillar>>& pillarsByRegion,
        double                                                    pillarSize);

    // --- Geometry helpers ---
    [[nodiscard]] static Clipper2Lib::Path64 makeCircle(int64_t cx, int64_t cy,
                                                         int64_t radius,
                                                         int     sides) noexcept;
    [[nodiscard]] static double taperedRadius(double heightFraction,
                                              double baseRadius,
                                              double topRadius) noexcept;
};

} // namespace BP
} // namespace MarcSLM
