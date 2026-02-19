// ==============================================================================
// MarcSLM - Layer Height Generator
// ==============================================================================
// Responsibility: Compute the ordered sequence of Z-heights at which the mesh
//                 will be sliced, respecting machine resolution (z_steps_per_mm),
//                 first-layer thickness, and the object's physical height.
//
// This class has NO dependencies on mesh geometry, Clipper2, or any other
// heavy subsystem.  It operates purely on scalar configuration values.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/SlmConfig.hpp"

#include <cstddef>
#include <vector>

namespace MarcSLM {
namespace BuildPlate {

// ==============================================================================
// LayerHeightGenerator
// ==============================================================================

/// @brief Generates the Z-coordinate sequence for uniform SLM slicing.
///
/// @details Ported and isolated from the legacy `PrintObject::generate_object_layers`
///          method.  The generator:
///            - Enforces machine z-resolution via z_steps_per_mm quantisation.
///            - Inserts a first-layer height at the base.
///            - Adjusts the final layer so the stack reaches exactly @p objectHeight.
///            - Applies z-gradation rounding to the whole stack when configured.
///
/// ### Thread Safety
///   Stateless: every `generate()` call is independent and re-entrant.
class LayerHeightGenerator {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    LayerHeightGenerator() = default;
    ~LayerHeightGenerator() = default;

    LayerHeightGenerator(const LayerHeightGenerator&)            = default;
    LayerHeightGenerator& operator=(const LayerHeightGenerator&) = default;
    LayerHeightGenerator(LayerHeightGenerator&&) noexcept        = default;
    LayerHeightGenerator& operator=(LayerHeightGenerator&&) noexcept = default;

    // =========================================================================
    // Primary Interface
    // =========================================================================

    /// @brief Generate the ordered vector of absolute Z print-heights.
    ///
    /// @param config            SLM configuration holding layer_thickness,
    ///                          first_layer_thickness, and z_steps_per_mm.
    /// @param objectHeight      Total height of the object bounding box [mm].
    /// @param firstLayerHeight  Override for first-layer thickness; if <= 0
    ///                          config.first_layer_thickness is used.
    ///
    /// @return Monotonically increasing vector of Z-heights [mm], starting at
    ///         the first-layer height and ending at (approximately) objectHeight.
    ///         Returns an empty vector when objectHeight <= 0.
    [[nodiscard]] std::vector<double> generate(
        const SlmConfig& config,
        double           objectHeight,
        double           firstLayerHeight = 0.0) const;

private:
    // =========================================================================
    // Implementation Helpers
    // =========================================================================

    /// @brief Round a layer height to the nearest machine z-step.
    /// @param height         Raw layer height [mm].
    /// @param minDz          Minimum Z step = 1 / z_steps_per_mm [mm].
    /// @return               Quantised height (always > 0).
    [[nodiscard]] static double quantise(double height, double minDz) noexcept;

    /// @brief Apply z-gradation rounding to an already-built height stack.
    /// @param heights   In-out vector; modified in place.
    /// @param gradation Step granularity [mm] = 1 / z_steps_per_mm.
    static void applyGradation(std::vector<double>& heights,
                               double               gradation) noexcept;

    /// @brief Adjust the final layer of the stack to match objectHeight.
    /// @param heights       In-out vector; last element may be changed.
    /// @param objectHeight  Target height [mm].
    /// @param nominalH      Nominal layer height [mm] used to clamp the
    ///                      adjustment within [0.5×nominal, 1.5×nominal].
    static void adjustLastLayer(std::vector<double>& heights,
                                double               objectHeight,
                                double               nominalH) noexcept;
};

} // namespace BuildPlate
} // namespace MarcSLM
