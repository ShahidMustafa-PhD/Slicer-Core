// ==============================================================================
// MarcSLM - Mesh I/O Module (High-Performance 3D Model Loading)
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// Robust mesh loading using Assimp with industrial-grade validation:
// - Supported formats: STL (Binary/ASCII), OBJ, AMF, 3MF, PLY, FBX
// - Post-processing: Triangulation, vertex welding, degenerate removal
// - Manifold validation: Non-manifold edge detection and reporting
// - Zero-copy Eigen integration for numerical efficiency
// ==============================================================================

#pragma once

#include "MeshData.hpp"
#include <string>
#include <vector>
#include <stdexcept>

namespace MarcSLM {
namespace IO {

// ==============================================================================
// Exception Hierarchy
// ==============================================================================

/// @brief Base exception for all mesh I/O errors.
class MeshIOError : public std::runtime_error {
public:
    explicit MeshIOError(const std::string& message)
        : std::runtime_error(message) {}
};

/// @brief Exception thrown when file loading fails.
class MeshLoadError : public MeshIOError {
public:
    MeshLoadError(const std::string& filePath, const std::string& reason)
        : MeshIOError("Failed to load '" + filePath + "': " + reason),
          filePath_(filePath), reason_(reason) {}
    
    [[nodiscard]] const std::string& filePath() const noexcept { return filePath_; }
    [[nodiscard]] const std::string& reason() const noexcept { return reason_; }
    
private:
    std::string filePath_;
    std::string reason_;
};

/// @brief Exception thrown when mesh validation fails.
class MeshValidationError : public MeshIOError {
public:
    explicit MeshValidationError(const std::string& details)
        : MeshIOError("Mesh validation failed: " + details),
          details_(details) {}
    
    [[nodiscard]] const std::string& details() const noexcept { return details_; }
    
private:
    std::string details_;
};

/// @brief Exception thrown when non-manifold geometry is detected.
class NonManifoldMeshError : public MeshValidationError {
public:
    NonManifoldMeshError(size_t nonManifoldEdgeCount, const std::string& details)
        : MeshValidationError("Non-manifold mesh detected: " + std::to_string(nonManifoldEdgeCount) +
                             " problematic edges. " + details),
          nonManifoldEdgeCount_(nonManifoldEdgeCount) {}
    
    [[nodiscard]] size_t nonManifoldEdgeCount() const noexcept { return nonManifoldEdgeCount_; }
    
private:
    size_t nonManifoldEdgeCount_;
};

// ==============================================================================
// Mesh I/O Class
// ==============================================================================

/// @brief High-performance mesh loading and validation for SLM/DMLS slicing.
/// @details This class provides:
///          - Multi-format mesh loading via Assimp
///          - Robust post-processing and validation
///          - Manifold topology checking
///          - Zero-copy Eigen matrix output
///          - Detailed error reporting
///
/// @par Supported Formats:
///      - STL (Binary/ASCII)
///      - OBJ (Wavefront)
///      - AMF (Additive Manufacturing Format)
///      - 3MF (3D Manufacturing Format)
///      - PLY (Stanford Polygon)
///      - FBX (Autodesk)
///
/// @par Post-Processing Pipeline:
///      1. Triangulate all polygons
///      2. Join identical vertices (weld)
///      3. Remove degenerate triangles
///      4. Improve cache locality
///      5. Validate manifold topology
///
/// @par Thread Safety:
///      - loadFromFile() is thread-safe (no shared state)
///      - Multiple instances can load files concurrently
///
/// @par Example Usage:
/// @code
///     MarcSLM::IO::MeshIO loader;
///     MarcSLM::IO::MeshData mesh;
///     
///     try {
///         if (loader.loadFromFile("part.stl", mesh)) {
///             std::cout << "Loaded " << mesh.vertexCount() << " vertices\n";
///             std::cout << "Loaded " << mesh.faceCount() << " faces\n";
///         }
///     } catch (const MarcSLM::IO::MeshLoadError& e) {
///         std::cerr << "Load failed: " << e.what() << std::endl;
///     }
/// @endcode
class MeshIO {
public:
    // ==========================================================================
    // Construction
    // ==========================================================================
    
    /// @brief Default constructor.
    MeshIO() = default;
    
    /// @brief Destructor.
    ~MeshIO() = default;
    
    // Disable copy (enforce move-only semantics if needed)
    MeshIO(const MeshIO&) = delete;
    MeshIO& operator=(const MeshIO&) = delete;
    
    // Enable move
    MeshIO(MeshIO&&) noexcept = default;
    MeshIO& operator=(MeshIO&&) noexcept = default;
    
    // ==========================================================================
    // Mesh Loading
    // ==========================================================================
    
    /// @brief Load a 3D mesh from file into MeshData structure.
    /// @param filePath Path to mesh file (STL, OBJ, AMF, etc.)
    /// @param outData Output mesh data (cleared before loading)
    /// @return True on success, false on failure (check lastError())
    /// @throws MeshLoadError if file cannot be loaded
    /// @throws MeshValidationError if mesh is invalid
    /// @throws NonManifoldMeshError if non-manifold edges are detected
    ///
    /// @par Post-Processing Applied:
    ///      - aiProcess_Triangulate: Convert all polygons to triangles
    ///      - aiProcess_JoinIdenticalVertices: Weld duplicate vertices
    ///      - aiProcess_ImproveCacheLocality: Optimize vertex cache usage
    ///      - aiProcess_FindDegenerates: Remove degenerate triangles
    ///      - aiProcess_FixInfacingNormals: Correct inverted normals
    ///
    /// @note This method computes face normals automatically.
    [[nodiscard]] bool loadFromFile(const std::string& filePath, MeshData& outData);
    
    // ==========================================================================
    // Validation
    // ==========================================================================
    
    /// @brief Validate mesh for manifoldness (optional, called internally).
    /// @param data Mesh data to validate
    /// @return True if mesh is manifold
    /// @throws NonManifoldMeshError if validation fails
    ///
    /// @details A manifold mesh satisfies:
    ///          1. Every edge is shared by exactly 2 faces
    ///          2. Faces around a vertex form a single fan
    ///          3. No duplicate faces
    ///
    /// @note This is a topological check, not a geometric check.
    [[nodiscard]] bool validateManifold(const MeshData& data);
    
    // ==========================================================================
    // Utility Methods
    // ==========================================================================
    
    /// @brief Get last error message (if loadFromFile returned false).
    /// @return Human-readable error description
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }
    
    /// @brief Check if a file format is supported.
    /// @param extension File extension (e.g., ".stl", ".obj")
    /// @return True if format is supported
    [[nodiscard]] static bool isSupportedFormat(const std::string& extension);
    
    /// @brief Get list of all supported file extensions.
    /// @return Vector of extensions (e.g., {".stl", ".obj", ".amf"})
    [[nodiscard]] static std::vector<std::string> supportedFormats();
    
private:
    // ==========================================================================
    // Internal Methods
    // ==========================================================================
    
    /// @brief Compute face normals from vertex positions and face indices.
    /// @param V Vertex matrix (N x 3)
    /// @param F Face index matrix (M x 3)
    /// @return Normal matrix (M x 3), normalized unit vectors
    static Eigen::MatrixXd computeFaceNormals(const Eigen::MatrixXd& V,
                                              const Eigen::MatrixXi& F);
    
    /// @brief Detect non-manifold edges in mesh topology.
    /// @param F Face index matrix (M x 3)
    /// @return Count of non-manifold edges (0 = manifold)
    static size_t detectNonManifoldEdges(const Eigen::MatrixXi& F);
    
    /// @brief Extract file extension from path.
    /// @param filePath Full file path
    /// @return Lowercase extension (e.g., ".stl")
    static std::string getFileExtension(const std::string& filePath);
    
    // ==========================================================================
    // Member Variables
    // ==========================================================================
    
    std::string lastError_;  ///< Last error message (empty if no error)
};

} // namespace IO
} // namespace MarcSLM
