// ==============================================================================
// MarcSLM - Model Placement
// ==============================================================================
// Responsibility: Encapsulates the placement transform for a single model on
//                 the build plate.  Stores position + Euler orientation and
//                 can apply the combined transform to a MeshProcessor.
//
// Design note:  This is a pure value type.  It owns no resources and has no
//               side-effects.  The actual mesh mutation happens through
//               MeshProcessor::applyPlacement which this class wraps with
//               validation and logging.
//
// Ported conceptually from Legacy Slic3r::ModelInstance (position + rotation)
// but simplified for the SLM use-case where each model has exactly one
// instance on the build plate.
// ==============================================================================

#pragma once

#include "MarcSLM/Core/InternalModel.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"

#include <cmath>
#include <cstdint>
#include <string>

namespace MarcSLM {

// Forward declaration
namespace Geometry { class MeshProcessor; }

namespace BP {

// ==============================================================================
// ModelPlacement  — placement transform for one model on the build plate
// ==============================================================================

/// @brief Describes where and how a model is placed on the build plate.
///
/// @details Conceptually equivalent to Legacy Slic3r's ModelInstance but
///          tailored for SLM where each model has exactly one placement
///          (no multi-instance duplication needed).
///
///          Coordinate convention:
///          - Position is in mm, relative to the build plate origin (bottom-left).
///          - Euler angles are in radians, applied in intrinsic ZYX order
///            (yaw ? pitch ? roll).
///          - After rotation the model is re-grounded (Z-min ? 0).
struct ModelPlacement {
    // =========================================================================
    // Identification
    // =========================================================================

    uint32_t    modelId   = 0;         ///< Unique ID on the build plate
    std::string modelPath;             ///< Path to source file (for diagnostics)

    // =========================================================================
    // Positional Placement  [mm]
    // =========================================================================

    double x = 0.0;                    ///< X offset on build plate
    double y = 0.0;                    ///< Y offset on build plate
    double z = 0.0;                    ///< Z offset (usually 0 for SLM)

    // =========================================================================
    // Euler Orientation  [radians]
    // =========================================================================

    double roll  = 0.0;               ///< Rotation about X-axis
    double pitch = 0.0;               ///< Rotation about Y-axis
    double yaw   = 0.0;               ///< Rotation about Z-axis

    // =========================================================================
    // Construction
    // =========================================================================

    ModelPlacement() = default;

    /// @brief Construct from an InternalModel descriptor.
    explicit ModelPlacement(const InternalModel& model)
        : modelId(static_cast<uint32_t>(model.number))
        , modelPath(model.path)
        , x(model.xpos), y(model.ypos), z(model.zpos)
        , roll(model.roll), pitch(model.pitch), yaw(model.yaw)
    {}

    // =========================================================================
    // Queries
    // =========================================================================

    /// @brief True if any Euler angle is non-zero.
    [[nodiscard]] bool hasRotation() const noexcept {
        constexpr double eps = 1e-9;
        return std::abs(roll) > eps || std::abs(pitch) > eps || std::abs(yaw) > eps;
    }

    /// @brief True if the position offset is non-zero.
    [[nodiscard]] bool hasTranslation() const noexcept {
        constexpr double eps = 1e-9;
        return std::abs(x) > eps || std::abs(y) > eps || std::abs(z) > eps;
    }

    /// @brief True if any transform component is non-identity.
    [[nodiscard]] bool hasTransform() const noexcept {
        return hasRotation() || hasTranslation();
    }

    // =========================================================================
    // Transform Application
    // =========================================================================

    /// @brief Apply this placement to a MeshProcessor.
    ///
    /// @details Applies Euler rotations (ZYX order), aligns to ground,
    ///          then translates.  This matches MeshProcessor::applyPlacement.
    ///
    /// @param processor  The mesh to transform.
    /// @throws MeshProcessingError if no valid mesh is loaded.
    void applyTo(Geometry::MeshProcessor& processor) const;

    /// @brief Compute the bounding box that results from applying this
    ///        placement to the given source bounding box.
    ///
    /// @note  This is an approximation — rotations expand the AABB.
    ///        For exact results, applyTo() + getBoundingBox() is needed.
    [[nodiscard]] Geometry::BBox3f estimateTransformedBBox(
        const Geometry::BBox3f& sourceBBox) const noexcept;
};

} // namespace BP
} // namespace MarcSLM
