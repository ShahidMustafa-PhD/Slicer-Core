// ==============================================================================
// MarcSLM - Mesh Processor: High-Performance 3D Mesh Slicing Engine
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// This class bridges Assimp (3D model loading) ? TriMesh (mesh repair) ?
// Clipper2 (2D polygon processing) for industrial SLM slicing.
// ==============================================================================

#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <tbb/parallel_for.h>

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>
#include <optional>

namespace MarcSLM {
namespace Geometry {

// ==============================================================================
// Forward Declarations and Type Aliases
// ==============================================================================

/// @brief Result type for slicing operations
using LayerSlices = Marc::Layer;

/// @brief Collection of sliced layers representing a complete build
using LayerStack = std::vector<LayerSlices>;

// ==============================================================================
// Exception Classes
// ==============================================================================

/// @brief Base exception for mesh processing errors
class MeshProcessingError : public std::runtime_error {
public:
    explicit MeshProcessingError(const std::string& msg)
        : std::runtime_error(msg) {}
};

/// @brief Thrown when input mesh has topological issues
class NonManifoldMeshError : public MeshProcessingError {
public:
    explicit NonManifoldMeshError(const std::string& details)
        : MeshProcessingError("Non-manifold mesh: " + details) {}
};

/// @brief Thrown when mesh loading fails
class MeshLoadError : public MeshProcessingError {
public:
    explicit MeshLoadError(const std::string& filePath, const std::string& reason)
        : MeshProcessingError("Failed to load mesh '" + filePath + "': " + reason) {}
};

/// @brief Thrown when slicing operation fails
class SlicingError : public MeshProcessingError {
public:
    explicit SlicingError(const std::string& details)
        : MeshProcessingError("Slicing failed: " + details) {}
};

// ==============================================================================
// Mesh Processor Class
// ==============================================================================

/// @brief High-performance 3D mesh slicing engine for industrial SLM/DMLS
/// @details Uses admesh-inspired topology repair and direct triangle-plane
///          intersection (ported from Legacy Slic3r code) instead of Manifold.
///          This approach handles non-manifold STL files robustly.
///
/// Pipeline Overview:
///   1. Load 3D mesh (Assimp) ? TriMesh
///   2. Repair topology (edge matching, degenerate removal, normal fixing)
///   3. Slice at Z-heights (triangle-plane intersection)
///   4. Chain intersection lines ? polygon loops
///   5. Classify contours vs holes using Clipper2
///   6. Generate Marc::Layer objects with all geometry
///
/// Thread Safety:
///   - Loading: Not thread-safe (Assimp::Importer state)
///   - Slicing: Thread-safe per-layer (TBB parallel processing)
///   - Access: Thread-safe for immutable operations
class MeshProcessor {
public:
    MeshProcessor() noexcept = default;
    ~MeshProcessor() noexcept = default;

    MeshProcessor(const MeshProcessor&) = delete;
    MeshProcessor& operator=(const MeshProcessor&) = delete;
    MeshProcessor(MeshProcessor&&) noexcept = default;
    MeshProcessor& operator=(MeshProcessor&&) noexcept = default;

    // =========================================================================
    // Mesh Loading and Validation
    // =========================================================================

    /// @brief Load 3D mesh from file, convert to TriMesh, and repair topology.
    /// @param filePath Path to 3D model file (STL, 3MF, OBJ, etc.)
    /// @throws MeshLoadError if file cannot be loaded or parsed
    void loadMesh(const std::string& filePath);

    /// @brief Check if a valid mesh is currently loaded
    [[nodiscard]] bool hasValidMesh() const noexcept {
        return mesh_ && !mesh_->empty();
    }

    /// @brief Get bounding box of loaded mesh
    /// @throws MeshProcessingError if no mesh is loaded
    [[nodiscard]] BBox3f getBoundingBox() const;

    /// @brief Get mesh statistics {vertexCount, triangleCount, volume}
    /// @throws MeshProcessingError if no mesh is loaded
    [[nodiscard]] std::tuple<size_t, size_t, double> getMeshStats() const;

    // =========================================================================
    // Slicing Algorithms
    // =========================================================================

    /// @brief Perform uniform-thickness slicing
    /// @param layerThickness Z-height increment in millimeters
    [[nodiscard]] LayerStack sliceUniform(float layerThickness);

    /// @brief Perform adaptive-thickness slicing
    [[nodiscard]] LayerStack sliceAdaptive(float minHeight, float maxHeight,
                                           float maxError = 0.05f);

    /// @brief Slice single layer at specific Z-height
    [[nodiscard]] LayerSlices sliceAtHeight(float zHeight);

    /// @brief Slice multiple specific Z-heights
    [[nodiscard]] LayerStack sliceAtHeights(const std::vector<float>& zHeights);

public:
    /// @brief The loaded and repaired 3D mesh
    std::unique_ptr<TriMesh> mesh_;

    /// @brief Cached bounding box
    mutable std::optional<BBox3f> bbox_;

private:
    // =========================================================================
    // Private Helper Methods
    // =========================================================================

    /// @brief Convert ExPolygon2i (scaled integer) to Marc::Polyline (float mm)
    [[nodiscard]] LayerSlices
    createLayerFromExPolygons(const ExPolygons2i& exPolygons,
                              float zHeight, uint32_t layerIndex,
                              float layerThickness) const;

    /// @brief Compute adaptive layer height based on cross-section change
    [[nodiscard]] float computeAdaptiveHeight(float z, float delta,
                                              float maxError,
                                              float minHeight,
                                              float maxHeight) const;

    void validateSlicingParams(float layerThickness) const;
    void validateAdaptiveParams(float minHeight, float maxHeight,
                               float maxError) const;
};

} // namespace Geometry
} // namespace MarcSLM
