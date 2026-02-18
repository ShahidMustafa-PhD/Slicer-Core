// ==============================================================================
// MarcSLM - Mesh Processor: High-Performance 3D Mesh Slicing Engine
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// This class bridges Assimp (3D model loading) ? Manifold (mesh slicing) ?
// Clipper2 (2D polygon processing) for industrial SLM slicing.
// ==============================================================================

#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <manifold/manifold.h>
#include <manifold/common.h>
#include <tbb/parallel_for.h>

#include "MarcSLM/Core/MarcFormat.hpp"

#include <memory>
#include <vector>
#include <string>
#include <stdexcept>

namespace MarcSLM {
namespace Geometry {

// ==============================================================================
// Forward Declarations and Type Aliases
// ==============================================================================

/// @brief Result type for slicing operations
/// @details Each Layer contains all geometry (hatches, polylines, polygons, circles)
///          for a single Z-height in the build.
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

/// @brief Thrown when input mesh is not manifold or has topological issues
class NonManifoldMeshError : public MeshProcessingError {
public:
    explicit NonManifoldMeshError(const std::string& details)
        : MeshProcessingError("Non-manifold mesh: " + details) {}
};

/// @brief Thrown when mesh loading fails (file not found, corrupt data, etc.)
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
/// @details This class is the heart of the MarcSLM slicer. It loads 3D models
///          using Assimp, converts them to watertight Manifold solids, and
///          performs high-performance Z-plane slicing with both uniform and
///          adaptive layer thickness algorithms.
///
/// Pipeline Overview:
///   1. Load 3D mesh (Assimp) ? Manifold solid
///   2. Validate manifoldness and repair if needed
///   3. Slice at Z-heights (uniform or adaptive)
///   4. Convert 3D contours ? 2D Clipper2 polygons
///   5. Generate Marc::Layer objects with all geometry
///
/// Thread Safety:
///   - Loading: Not thread-safe (Assimp::Importer state)
///   - Slicing: Thread-safe (parallel layer processing)
///   - Access: Thread-safe for immutable operations
///
/// Performance Characteristics:
///   - Target: >100 layers/second for 100k-1M triangle meshes
///   - Memory: <2GB for 10k layers with moderate complexity
///   - Parallelism: Intel TBB for multi-core scaling
class MeshProcessor {
public:
    // =========================================================================
    // Construction and Configuration
    // =========================================================================

    /// @brief Default constructor with sensible defaults
    MeshProcessor() noexcept = default;

    /// @brief Destructor
    ~MeshProcessor() noexcept = default;

    // Deleted copy operations (expensive mesh data)
    MeshProcessor(const MeshProcessor&) = delete;
    MeshProcessor& operator=(const MeshProcessor&) = delete;

    // Move operations allowed
    MeshProcessor(MeshProcessor&&) noexcept = default;
    MeshProcessor& operator=(MeshProcessor&&) noexcept = default;

    // =========================================================================
    // Mesh Loading and Validation
    // =========================================================================

    /// @brief Load 3D mesh from file and convert to watertight Manifold solid
    /// @param filePath Path to 3D model file (STL, 3MF, OBJ, etc.)
    /// @return Unique pointer to validated Manifold solid
    /// @throws MeshLoadError if file cannot be loaded or parsed
    /// @throws NonManifoldMeshError if mesh has topological issues
    ///
    /// Processing Steps:
    ///   1. Load with Assimp (triangulate, join vertices, fix normals)
    ///   2. Extract vertex/triangle data into manifold::MeshGL
    ///   3. Create manifold::Manifold and validate
    ///   4. Repair non-manifold edges if possible
    ///
    /// Supported Formats: STL, 3MF, OBJ, PLY, FBX (via Assimp)
    [[nodiscard]] std::unique_ptr<manifold::Manifold>
    loadMesh(const std::string& filePath);

    /// @brief Check if a valid mesh is currently loaded
    /// @return true if mesh is loaded and manifold
    [[nodiscard]] bool hasValidMesh() const noexcept {
        return mesh_ && mesh_->Status() == manifold::Manifold::Error::NoError;
    }

    /// @brief Get bounding box of loaded mesh
    /// @return Axis-aligned bounding box in millimeters
    /// @throws MeshProcessingError if no mesh is loaded
    [[nodiscard]] manifold::Box getBoundingBox() const;

    /// @brief Get mesh statistics
    /// @return Triple {vertexCount, triangleCount, volume}
    /// @throws MeshProcessingError if no mesh is loaded
    [[nodiscard]] std::tuple<size_t, size_t, double> getMeshStats() const;

    // =========================================================================
    // Slicing Algorithms
    // =========================================================================

    /// @brief Perform uniform-thickness slicing
    /// @param layerThickness Z-height increment in millimeters
    /// @return Vector of LayerSlices, one per Z-height
    /// @throws SlicingError if slicing fails
    /// @throws MeshProcessingError if no valid mesh is loaded
    ///
    /// Algorithm:
    ///   - Slice from mesh bottom to top with constant thickness
    ///   - Parallel processing using Intel TBB
    ///   - Each layer contains all geometry types (hatches, contours, etc.)
    ///
    /// Performance: O(numLayers * meshComplexity)
    [[nodiscard]] LayerStack sliceUniform(float layerThickness);

    /// @brief Perform adaptive-thickness slicing (Slic3r-inspired)
    /// @param minHeight Minimum layer thickness in mm (e.g., 0.01)
    /// @param maxHeight Maximum layer thickness in mm (e.g., 0.10)
    /// @param maxError Maximum allowed geometric deviation (0.0-1.0)
    /// @return Vector of LayerSlices with variable Z-heights
    /// @throws SlicingError if slicing fails
    /// @throws MeshProcessingError if no valid mesh is loaded
    ///
    /// Adaptive Algorithm:
    ///   - Start with maxHeight
    ///   - At each Z, compare cross-section area/boundary at Z vs Z+?
    ///   - If deviation > maxError, reduce layer height (down to minHeight)
    ///   - If deviation small, increase layer height (up to maxHeight)
    ///   - Balances speed vs. geometric accuracy
    ///
    /// Performance: O(numLayers * meshComplexity * 2) (two slices per layer)
    [[nodiscard]] LayerStack sliceAdaptive(float minHeight, float maxHeight,
                                           float maxError = 0.05f);

    // =========================================================================
    // Advanced Slicing Options
    // =========================================================================

    /// @brief Slice single layer at specific Z-height
    /// @param zHeight Z-coordinate in millimeters
    /// @return Single LayerSlices object
    /// @throws SlicingError if slicing fails
    /// @throws MeshProcessingError if no valid mesh is loaded
    [[nodiscard]] LayerSlices sliceAtHeight(float zHeight);

    /// @brief Slice multiple specific Z-heights
    /// @param zHeights Vector of Z-coordinates in millimeters
    /// @return Vector of LayerSlices (same order as zHeights)
    /// @throws SlicingError if any slicing fails
    /// @throws MeshProcessingError if no valid mesh is loaded
    [[nodiscard]] LayerStack sliceAtHeights(const std::vector<float>& zHeights);

public:
    // =========================================================================
    // Public Data Members (for testing/access)
    // =========================================================================

    /// @brief The loaded and validated 3D mesh
    std::unique_ptr<manifold::Manifold> mesh_;

    /// @brief Cached bounding box (computed once after loading)
    mutable std::optional<manifold::Box> bbox_;

private:
    // =========================================================================
    // Private Helper Methods
    // =========================================================================

    /// @brief Convert Assimp mesh to Manifold MeshGL structure
    /// @param mesh Assimp mesh pointer (must be triangulated)
    /// @return Manifold MeshGL ready for Manifold construction
    [[nodiscard]] manifold::MeshGL assimpToManifoldMesh(const aiMesh* mesh) const;

    /// @brief Convert Manifold CrossSection to Marc::Slice
    /// @param cs Manifold cross-section at single Z-height
    /// @param zHeight Z-coordinate for metadata
    /// @param layerIndex Layer index for metadata
    /// @return Marc::Slice with contour and holes
    ///
    /// Conversion Process:
    ///   1. Extract polygons from CrossSection using ToPolygons()
    ///   2. Convert glm::vec2 ? Clipper2Lib::Point64 (scale by 1000)
    ///   3. Use Clipper2Lib::PolyTree64 to identify contours vs holes
    ///   4. Populate Marc::Core::Slice structure
    [[nodiscard]] Marc::Slice
    manifoldToMarcSlice(const manifold::Polygons& polygons,
                       double zHeight, uint32_t layerIndex) const;

    /// @brief Create LayerSlices from cross-section polygons
    /// @param polygons Manifold polygons at single Z-height
    /// @param zHeight Z-coordinate
    /// @param layerIndex Layer index
    /// @param layerThickness Thickness for this layer
    /// @return Complete LayerSlices with all geometry populated
    ///
    /// Note: Currently populates only outer contour as polylines.
    ///       Future: Add hatching, infill, support generation.
    [[nodiscard]] LayerSlices
    createLayerFromPolygons(const manifold::Polygons& polygons,
                           float zHeight, uint32_t layerIndex,
                           float layerThickness);

    /// @brief Compute adaptive layer height based on geometric deviation
    /// @param z Current Z-height
    /// @param delta Small offset for comparison (e.g., 0.001 mm)
    /// @param maxError Maximum allowed deviation (0.0-1.0)
    /// @param minHeight Minimum layer height
    /// @param maxHeight Maximum layer height
    /// @return Recommended layer height for next step
    [[nodiscard]] float computeAdaptiveHeight(float z, float delta,
                                              float maxError,
                                              float minHeight, float maxHeight) const;

    /// @brief Validate slicing parameters
    /// @param layerThickness Layer thickness to validate
    /// @throws SlicingError if parameters are invalid
    void validateSlicingParams(float layerThickness) const;

    /// @brief Validate adaptive slicing parameters
    /// @param minHeight Minimum height
    /// @param maxHeight Maximum height
    /// @param maxError Maximum error
    /// @throws SlicingError if parameters are invalid
    void validateAdaptiveParams(float minHeight, float maxHeight,
                               float maxError) const;
};

} // namespace Geometry
} // namespace MarcSLM
