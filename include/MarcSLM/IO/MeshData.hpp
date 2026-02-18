// ==============================================================================
// MarcSLM - Mesh Data Structure (Data-Oriented Design)
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// Data-oriented mesh representation using Eigen matrices for zero-copy
// efficiency and direct integration with numerical algorithms.
// ==============================================================================

#pragma once

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <string>

namespace MarcSLM {
namespace IO {

/// @brief Data-oriented mesh representation for high-performance processing.
/// @details Uses Eigen matrices for efficient numerical operations:
///          - V: Vertex positions (N x 3) in millimeters
///          - F: Triangle face indices (M x 3), 0-indexed
///          - N: Face normals (M x 3), normalized unit vectors
///
///          This structure is designed for zero-copy data transfer from
///          Assimp to downstream processing algorithms.
///
/// @note Thread-safety: Read-only access is thread-safe. Modifications
///       require external synchronization.
struct MeshData {
    // ==========================================================================
    // Geometry Data
    // ==========================================================================
    
    /// Vertex positions [mm] (N rows x 3 columns: X, Y, Z)
    Eigen::MatrixXd V;
    
    /// Triangle face indices (M rows x 3 columns: i0, i1, i2)
    /// Each index references a row in V. Faces are CCW-wound.
    Eigen::MatrixXi F;
    
    /// Face normals (M rows x 3 columns: nx, ny, nz)
    /// Normalized unit vectors. Computed during loading.
    Eigen::MatrixXd N;
    
    // ==========================================================================
    // Metadata
    // ==========================================================================
    
    std::string sourcePath;       ///< Original file path
    std::string formatHint;       ///< File format (e.g., "STL", "OBJ", "AMF")
    
    // ==========================================================================
    // Utility Methods
    // ==========================================================================
    
    /// @brief Check if mesh data is empty.
    /// @return True if no vertices or faces are present.
    [[nodiscard]] bool isEmpty() const noexcept {
        return V.rows() == 0 || F.rows() == 0;
    }
    
    /// @brief Get number of vertices.
    [[nodiscard]] size_t vertexCount() const noexcept {
        return static_cast<size_t>(V.rows());
    }
    
    /// @brief Get number of triangular faces.
    [[nodiscard]] size_t faceCount() const noexcept {
        return static_cast<size_t>(F.rows());
    }
    
    /// @brief Compute axis-aligned bounding box.
    /// @return [minX, minY, minZ, maxX, maxY, maxZ] in millimeters.
    [[nodiscard]] Eigen::RowVector<double, 6> boundingBox() const {
        if (isEmpty()) {
            return Eigen::RowVector<double, 6>::Zero();
        }
        
        Eigen::RowVector3d minPt = V.colwise().minCoeff();
        Eigen::RowVector3d maxPt = V.colwise().maxCoeff();
        
        Eigen::RowVector<double, 6> bbox;
        bbox << minPt, maxPt;
        return bbox;
    }
    
    /// @brief Get bounding box dimensions [width, depth, height] in mm.
    [[nodiscard]] Eigen::Vector3d dimensions() const {
        if (isEmpty()) {
            return Eigen::Vector3d::Zero();
        }
        
        Eigen::RowVector3d minPt = V.colwise().minCoeff();
        Eigen::RowVector3d maxPt = V.colwise().maxCoeff();
        return (maxPt - minPt).transpose();
    }
    
    /// @brief Clear all mesh data.
    void clear() noexcept {
        V.resize(0, 3);
        F.resize(0, 3);
        N.resize(0, 3);
        sourcePath.clear();
        formatHint.clear();
    }
    
    /// @brief Reserve memory for known mesh size (optimization).
    /// @param numVertices Expected number of vertices.
    /// @param numFaces Expected number of faces.
    void reserve(size_t numVertices, size_t numFaces) {
        V.resize(numVertices, 3);
        F.resize(numFaces, 3);
        N.resize(numFaces, 3);
    }
};

} // namespace IO
} // namespace MarcSLM
