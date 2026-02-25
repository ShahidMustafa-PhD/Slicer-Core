// ==============================================================================
// MarcSLM - Thermal Region Hatcher
// ==============================================================================
// PySLM Stage 6: Generate actual parallel scan vectors (hatches) inside each
// classified thermal region boundary.
//
// PySLM hatching strategies (implemented here):
//   1. BasicHatcher     — Uniform parallel lines at fixed spacing and angle.
//   2. StripeHatcher    — Divides the region into horizontal/vertical stripes
//                         and hatches each stripe independently.
//   3. IslandHatcher    — Divides the region into a checkerboard grid of
//                         island cells, alternating hatch angle 0°/90° per cell
//                         to reduce residual stress (default SLM strategy).
//
// Each ThermalSegmentType maps to:
//   - A hatch spacing (contour types use finer spacing than core hatch).
//   - A hatch strategy (islands for core, basic for shells/contours).
//   - A base angle that rotates 67° per layer.
//
// Thread-safe: all methods are const; safe for concurrent layer processing.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"

#include <clipper2/clipper.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace MarcSLM {
namespace Thermal {

// ==============================================================================
// Hatch Strategy Enum
// ==============================================================================

/// @brief The hatching strategy to apply inside a thermal region.
enum class HatchStrategy : uint8_t {
    Basic  = 0,  ///< Uniform parallel lines (shells, contourHatch, support)
    Stripe = 1,  ///< Parallel stripes (alternative core strategy)
    Island = 2   ///< Checkerboard islands (default core hatch — PySLM default)
};

// ==============================================================================
// Per-Region Hatch Parameters
// ==============================================================================

/// @brief Resolved hatching parameters for a single thermal region.
struct RegionHatchParams {
    double         spacing   = 0.1;   ///< Line spacing [mm]
    double         angle     = 0.0;   ///< Resolved angle for this layer [deg]
    HatchStrategy  strategy  = HatchStrategy::Basic;
    double         islandW   = 5.0;   ///< Island cell width  [mm] (Island mode)
    double         islandH   = 5.0;   ///< Island cell height [mm] (Island mode)
    double         stripeW   = 10.0;  ///< Stripe width [mm]  (Stripe mode)
    double         overlap   = 0.0;   ///< Endpoint overlap extension [mm]
};

// ==============================================================================
// ThermalRegionHatcher
// ==============================================================================

/// @brief Generates actual parallel scan vectors inside classified thermal
///        region boundaries.
///
/// @details Implements the PySLM hatching pipeline:
///   1. Resolve per-segment-type hatch parameters from SlmConfig.
///   2. For each ClassifiedRegion, build Clipper2 subject/clip geometry.
///   3. Generate parallel lines covering the region bounding box.
///   4. Clip lines to the region boundary via Clipper2 open-path intersection.
///   5. Sort with alternating scan direction for thermal uniformity.
///
/// All coordinate conversion uses the Clipper2 scale factor from SlmConfig
/// (the same scale as the upstream segmentation pipeline).
class ThermalRegionHatcher {
public:
    /// @brief Construct from SLM configuration.
    explicit ThermalRegionHatcher(const SlmConfig& config) noexcept
        : hatchSpacing_(config.hatch_spacing)
        , baseAngle_(config.hatch_angle)
        , islandW_(config.island_width)
        , islandH_(config.island_height)
        , layerRotation_(67.0)
        , endpointOverlap_(0.0)
        , clipperScale_(1e4) {}

    /// @brief Set the Clipper2 scale factor (must match segmentation pipeline).
    void setClipperScale(double scale) noexcept { clipperScale_ = scale; }

    /// @brief Set the endpoint overlap extension [mm].
    void setEndpointOverlap(double mm) noexcept { endpointOverlap_ = mm; }

    /// @brief Set the per-layer rotation increment [degrees].
    void setLayerRotation(double deg) noexcept { layerRotation_ = deg; }

    // ---- Primary API -------------------------------------------------------

    /// @brief Generate hatch lines inside a thermal region.
    /// @param regionPaths  Clipper2 Paths64 boundary of the classified region
    ///                     (in Clipper2 integer units at clipperScale_).
    /// @param segType      The ThermalSegmentType of this region.
    /// @param layerIndex   Zero-based layer index (for angle rotation).
    /// @return Vector of Marc::Line in mm coordinates.
    [[nodiscard]] std::vector<Marc::Line>
    hatchRegion(const Clipper2Lib::Paths64& regionPaths,
                ThermalSegmentType segType,
                uint32_t layerIndex) const;

    /// @brief Resolve hatch parameters for a given segment type and layer.
    [[nodiscard]] RegionHatchParams
    resolveParams(ThermalSegmentType segType, uint32_t layerIndex) const;

private:
    double hatchSpacing_;
    double baseAngle_;
    double islandW_;
    double islandH_;
    double layerRotation_;
    double endpointOverlap_;
    double clipperScale_;

    // ---- Internal generation methods ---------------------------------------

    /// @brief Basic strategy: uniform parallel lines.
    [[nodiscard]] std::vector<Marc::Line>
    hatchBasic(const Clipper2Lib::Paths64& region,
               const RegionHatchParams& p) const;

    /// @brief Stripe strategy: divide into stripes, hatch each.
    [[nodiscard]] std::vector<Marc::Line>
    hatchStripe(const Clipper2Lib::Paths64& region,
                const RegionHatchParams& p) const;

    /// @brief Island strategy: checkerboard cells, alternating 0°/90°.
    [[nodiscard]] std::vector<Marc::Line>
    hatchIsland(const Clipper2Lib::Paths64& region,
                const RegionHatchParams& p) const;

    // ---- Helpers -----------------------------------------------------------

    /// @brief Generate parallel lines covering a bounding box.
    [[nodiscard]] std::vector<Marc::Line>
    generateParallelLines(double xMin, double yMin,
                          double xMax, double yMax,
                          double spacing, double angleDeg) const;

    /// @brief Clip open line paths against a closed polygon region.
    [[nodiscard]] std::vector<Marc::Line>
    clipLinesToRegion(const std::vector<Marc::Line>& lines,
                     const Clipper2Lib::Paths64& region) const;

    /// @brief Extend each line's endpoints by overlap distance.
    void applyOverlap(std::vector<Marc::Line>& lines) const;

    /// @brief Alternate scan direction for thermal uniformity.
    void sortAlternating(std::vector<Marc::Line>& lines) const;

    /// @brief Compute the mm bounding box of Clipper2 Paths64.
    void computeBoundingBox(const Clipper2Lib::Paths64& paths,
                            double& xMin, double& yMin,
                            double& xMax, double& yMax) const;

    /// @brief Convert Clipper2 int64 to mm.
    [[nodiscard]] double toMm(int64_t v) const noexcept {
        return static_cast<double>(v) / clipperScale_;
    }

    /// @brief Convert mm to Clipper2 int64.
    [[nodiscard]] int64_t toClip(double mm) const noexcept {
        return static_cast<int64_t>(std::round(mm * clipperScale_));
    }
};

} // namespace Thermal
} // namespace MarcSLM
