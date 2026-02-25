// ==============================================================================
// MarcSLM - Build Plate Preparator
// ==============================================================================
// Responsibility: Execute the complete build-plate preparation pipeline.
//
//   1. Apply per-model placement transforms (position + Euler rotation)
//   2. Validate no bounding-box overlaps (collision check)
//   3. Auto-arrange models on the bed if overlaps are detected
//   4. Validate all models fit within the bed dimensions
//   5. Align all models to Z=0 (ground)
//   6. Assign per-model region tags for identity preservation during
//      unified slicing
//
// This class is the SLM-specific equivalent of Legacy Slic3r's
//   Model::arrange_objects()  +  Print::apply_config()  pipeline.
//
// Design:
//   - Stateless service object — all state flows through the parameters.
//   - Operates on vectors of MeshProcessor + ModelPlacement.
//   - Does NOT own any of the resources it operates on.
//   - Thread safety: single-threaded (modifies meshes in place).
// ==============================================================================

#pragma once

#include "MarcSLM/Core/BuildPlate/ModelPlacement.hpp"
#include "MarcSLM/Geometry/MeshProcessor.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace MarcSLM {
namespace BP {

// ==============================================================================
// BuildPlatePreparator
// ==============================================================================

/// @brief Executes the build plate preparation pipeline.
///
/// @details Ported conceptually from Legacy Slic3r Model/Print:
///   - Model::arrange_objects()           ? arrangeModels()
///   - ModelObject::align_to_ground()     ? alignAllToGround()
///   - Model::bounding_box() validation   ? validateFitsInBed()
///   - Instance transform application     ? applyAllPlacements()
///
/// Usage:
/// @code
///   BuildPlatePreparator prep;
///   prep.setBedSize(120.0f, 120.0f);
///   prep.prepare(placements, processors); // throws on failure
/// @endcode
class BuildPlatePreparator {
public:
    /// @brief Progress callback: (message, percent 0-100).
    using ProgressCallback = std::function<void(const char*, int)>;

    BuildPlatePreparator() = default;
    ~BuildPlatePreparator() = default;

    // Non-copyable, non-movable (stateless service, no need)
    BuildPlatePreparator(const BuildPlatePreparator&)            = delete;
    BuildPlatePreparator& operator=(const BuildPlatePreparator&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Set the physical bed dimensions [mm].
    void setBedSize(float width, float depth) noexcept;

    /// @brief Set minimum spacing between models [mm].
    void setMinSpacing(double spacing) noexcept;

    /// @brief Set the progress callback.
    void setProgressCallback(ProgressCallback cb);

    // =========================================================================
    // Full Preparation Pipeline
    // =========================================================================

    /// @brief Execute the full preparation pipeline.
    ///
    /// @param placements   Per-model placement descriptors. Indices must
    ///                     correspond 1:1 with @p processors.
    /// @param processors   Per-model mesh processors (already loaded + repaired).
    ///                     These are mutated in-place (rotated, translated).
    ///
    /// @throws std::runtime_error on overlap that cannot be auto-arranged,
    ///         or if any model does not fit the bed.
    /// @throws Geometry::MeshProcessingError if a mesh is invalid.
    void prepare(std::vector<ModelPlacement>&                     placements,
                 std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors);

    // =========================================================================
    // Individual Steps  (public for testability)
    // =========================================================================

    /// @brief Apply each model's placement transform to its mesh.
    ///
    /// After this call every mesh is in build-plate coordinates
    /// (oriented, grounded, translated).
    void applyAllPlacements(
        const std::vector<ModelPlacement>&                     placements,
        std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const;

    /// @brief Ensure every model's Z-minimum is at 0.
    void alignAllToGround(
        std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const;

    /// @brief Validate that no two models overlap in XY.
    ///
    /// @param gap  Minimum gap between bounding boxes [mm].
    /// @throws std::runtime_error if overlap is detected.
    void validateNoOverlap(
        const std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors,
        float gap = 0.0f) const;

    /// @brief Validate that all models fit within the bed.
    ///
    /// @throws std::runtime_error if any model is out of bounds.
    void validateFitsInBed(
        const std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const;

    /// @brief Auto-arrange models using strip-packing to avoid overlaps.
    ///
    /// @param placements   Updated in-place with new positions.
    /// @param processors   Meshes are translated in-place.
    /// @param spacing      Gap between models [mm].  0 = use minSpacing_.
    /// @return true if all models were arranged successfully.
    bool arrangeModels(
        std::vector<ModelPlacement>&                           placements,
        std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors,
        double spacing = 0.0) const;

    // =========================================================================
    // Utilities
    // =========================================================================

    /// @brief Compute the combined bounding box of all processors.
    [[nodiscard]] Geometry::BBox3f combinedBoundingBox(
        const std::vector<std::unique_ptr<Geometry::MeshProcessor>>& processors) const;

    /// @brief Check if two AABBs overlap in XY with an optional gap.
    [[nodiscard]] static bool bboxOverlapXY(
        const Geometry::BBox3f& a,
        const Geometry::BBox3f& b,
        float gap = 0.0f) noexcept;

private:
    float  bedWidth_   = 120.0f;   ///< Build plate width  [mm] — matches BuildPlate default
    float  bedDepth_   = 120.0f;   ///< Build plate depth  [mm] — matches BuildPlate default
    double minSpacing_  = 5.0;     ///< Minimum gap between models [mm]
    ProgressCallback progressCb_;

    void reportProgress(const char* msg, int pct) const;
};

} // namespace BP
} // namespace MarcSLM
