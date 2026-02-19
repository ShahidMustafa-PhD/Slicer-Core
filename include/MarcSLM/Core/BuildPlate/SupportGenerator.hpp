// ==============================================================================
// MarcSLM - Support Generator
// ==============================================================================
// Responsibility: Given an OverhangMap, compute pillar support positions,
//                 determine their vertical extents, and stamp circular pillar
//                 footprints onto each BuildLayerRegion as SurfaceType::Support.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"
#include "MarcSLM/Core/BuildPlate/OverhangDetector.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"

#include <cstddef>
#include <map>
#include <vector>

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// SupportPillar (value type — no raw pointers, no owning heap)
// ==============================================================================

/// @brief Descriptor for a single tapered support pillar.
struct SupportPillar {
    double x           = 0.0;  ///< XY centre X [mm]
    double y           = 0.0;  ///< XY centre Y [mm]
    double radiusBase  = 0.0;  ///< Radius at the build plate [mm]
    double radiusTop   = 0.0;  ///< Radius at the top of the pillar [mm]
    std::size_t topLayer    = 0;   ///< Layer index where the pillar starts (top)
    std::size_t bottomLayer = 0;   ///< Layer index where the pillar ends  (bottom)
};

// ==============================================================================
// SupportGenerator
// ==============================================================================

/// @brief Generates tapered pillar support structures for a set of layers.
///
/// @details Ported and isolated from the legacy
///          `PrintObject::generatePillarPositions`,
///          `PrintObject::createSupportPillars`, and
///          `PrintObject::generateSupportGeometry` methods.
///
/// ### Pipeline (three stages):
///   1. **Pillar placement** — grid-sample the union of all overhang areas
///      at a spacing of `config.support_material_pillar_spacing`.
///   2. **Vertical extent** — walk layers top-to-bottom for each pillar
///      XY position to determine where the pillar begins and ends.
///   3. **Geometry stamping** — write tapered-circle ClassifiedSurface
///      polygons into each layer-region as `SurfaceType::Support`.
///
/// ### Thread Safety
///   Stateless: `generate()` is fully re-entrant provided the caller does not
///   concurrently write to the layers being modified.
class SupportGenerator {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    SupportGenerator() = default;
    ~SupportGenerator() = default;

    SupportGenerator(const SupportGenerator&)            = default;
    SupportGenerator& operator=(const SupportGenerator&) = default;
    SupportGenerator(SupportGenerator&&) noexcept        = default;
    SupportGenerator& operator=(SupportGenerator&&) noexcept = default;

    // =========================================================================
    // Primary Interface
    // =========================================================================

    /// @brief Build all support pillars and stamp geometry into layers.
    ///
    /// @param layers   Ordered layer stack (bottom ? top) to be modified.
    /// @param overhangs Overhang map from OverhangDetector::detect().
    /// @param config   SLM configuration for pillar sizing and spacing.
    void generate(const std::vector<BuildLayer*>& layers,
                  const OverhangMap&              overhangs,
                  const SlmConfig&                config) const;

private:
    // =========================================================================
    // Stage 1 – Pillar placement
    // =========================================================================

    /// @brief Grid-sample overhang areas to produce pillar XY positions.
    ///
    /// @param overhangs     Overhang geometry from OverhangDetector.
    /// @param pillarSpacing Grid spacing [mm].
    /// @return              Map of regionIdx ? list of Clipper2 pillar centres.
    [[nodiscard]] static std::map<std::size_t,
                                  std::vector<Clipper2Lib::Point64>>
    placePillars(const OverhangMap& overhangs, double pillarSpacing);

    // =========================================================================
    // Stage 2 – Vertical extent resolution
    // =========================================================================

    /// @brief Walk the layer stack per pillar XY to compute topLayer/bottomLayer.
    ///
    /// @param layers     Ordered layer stack.
    /// @param positions  Pillar centre positions from placePillars().
    /// @param pillarSize Base radius [mm] for bounding-box search.
    /// @return           Map of regionIdx ? list of fully-resolved SupportPillar.
    [[nodiscard]] static std::map<std::size_t, std::vector<SupportPillar>>
    resolveExtents(const std::vector<BuildLayer*>&                            layers,
                   const std::map<std::size_t,
                                  std::vector<Clipper2Lib::Point64>>& positions,
                   double                                              pillarSize);

    // =========================================================================
    // Stage 3 – Geometry stamping
    // =========================================================================

    /// @brief Write tapered-circle surfaces into the layer stack.
    ///
    /// @param layers        Layer stack to modify.
    /// @param pillarsByRegion Resolved pillar descriptors per region.
    /// @param pillarSize    Base diameter [mm].
    static void stampGeometry(
        const std::vector<BuildLayer*>&                            layers,
        const std::map<std::size_t, std::vector<SupportPillar>>& pillarsByRegion,
        double                                                     pillarSize);

    // =========================================================================
    // Geometry helpers
    // =========================================================================

    /// @brief Build a closed circular Clipper2 path centred at (cx, cy).
    ///
    /// @param cx       Centre X in Clipper2 integer units.
    /// @param cy       Centre Y in Clipper2 integer units.
    /// @param radius   Radius in Clipper2 integer units.
    /// @param sides    Number of polygon sides (minimum 8).
    [[nodiscard]] static Clipper2Lib::Path64 makeCircle(int64_t    cx,
                                                         int64_t    cy,
                                                         int64_t    radius,
                                                         int        sides) noexcept;

    /// @brief Compute the tapered radius at a fractional height in [0, 1].
    ///
    /// @param heightFraction  0 = base (largest), 1 = top (smallest).
    /// @param baseRadius      Radius at the base [mm].
    /// @param topRadius       Radius at the top  [mm].
    [[nodiscard]] static double taperedRadius(double heightFraction,
                                              double baseRadius,
                                              double topRadius) noexcept;
};

} // namespace BuildPlate
} // namespace MarcSLM
