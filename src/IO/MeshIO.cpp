// ==============================================================================
// MarcSLM - Mesh I/O Implementation
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================

#include "MarcSLM/IO/MeshIO.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

namespace MarcSLM {
namespace IO {

namespace {
    // ==========================================================================
    // Helper: Edge Hashing for Manifold Validation
    // ==========================================================================
    
    struct Edge {
        int v0, v1;  // Vertex indices (always sorted: v0 < v1)
        
        Edge(int a, int b) : v0(std::min(a, b)), v1(std::max(a, b)) {}
        
        bool operator==(const Edge& other) const {
            return v0 == other.v0 && v1 == other.v1;
        }
    };
    
    struct EdgeHash {
        std::size_t operator()(const Edge& e) const {
            // Cantor pairing function for edge hashing
            return (e.v0 + e.v1) * (e.v0 + e.v1 + 1) / 2 + e.v1;
        }
    };
    
} // anonymous namespace

// ==============================================================================
// Mesh Loading
// ==============================================================================

bool MeshIO::loadFromFile(const std::string& filePath, MeshData& outData) {
    // Clear previous data
    outData.clear();
    lastError_.clear();
    
    // Check file exists
    if (!std::filesystem::exists(filePath)) {
        lastError_ = "File not found: " + filePath;
        throw MeshLoadError(filePath, "File not found");
    }
    
    // Check format is supported
    std::string ext = getFileExtension(filePath);
    if (!isSupportedFormat(ext)) {
        lastError_ = "Unsupported format: " + ext;
        throw MeshLoadError(filePath, "Unsupported format: " + ext);
    }
    
    // Create Assimp importer
    Assimp::Importer importer;
    
    // Configure post-processing flags
    const unsigned int flags =
        aiProcess_Triangulate |              // Convert all polygons to triangles
        aiProcess_JoinIdenticalVertices |    // Weld duplicate vertices
        aiProcess_ImproveCacheLocality |     // Optimize vertex cache usage
        aiProcess_FindDegenerates |          // Remove degenerate triangles
        aiProcess_FixInfacingNormals |       // Correct inverted normals
        aiProcess_SortByPType |              // Group faces by type
        aiProcess_RemoveRedundantMaterials;  // Simplify materials
    
    // Load the scene
    const aiScene* scene = importer.ReadFile(filePath, flags);
    if (!scene) {
        lastError_ = importer.GetErrorString();
        throw MeshLoadError(filePath, lastError_);
    }
    
    // Validate scene structure
    if (!scene->HasMeshes() || scene->mNumMeshes == 0) {
        lastError_ = "Scene contains no meshes";
        throw MeshLoadError(filePath, lastError_);
    }
    
    // Process first mesh (most common case for SLM parts)
    // TODO: Support multi-mesh merging for assemblies
    const aiMesh* aiMesh = scene->mMeshes[0];
    if (!aiMesh) {
        lastError_ = "First mesh is null";
        throw MeshLoadError(filePath, lastError_);
    }
    
    // Validate mesh has required data
    if (!aiMesh->HasPositions()) {
        lastError_ = "Mesh has no vertex positions";
        throw MeshLoadError(filePath, lastError_);
    }
    
    if (!aiMesh->HasFaces()) {
        lastError_ = "Mesh has no faces";
        throw MeshLoadError(filePath, lastError_);
    }
    
    if (aiMesh->mNumVertices < 3 || aiMesh->mNumFaces == 0) {
        lastError_ = "Mesh is empty or degenerate";
        throw MeshLoadError(filePath, lastError_);
    }
    
    // Extract vertex data
    const size_t numVertices = aiMesh->mNumVertices;
    outData.V.resize(numVertices, 3);
    
    for (size_t i = 0; i < numVertices; ++i) {
        const aiVector3D& v = aiMesh->mVertices[i];
        outData.V(i, 0) = static_cast<double>(v.x);
        outData.V(i, 1) = static_cast<double>(v.y);
        outData.V(i, 2) = static_cast<double>(v.z);
    }
    
    // Extract face data (triangles only after aiProcess_Triangulate)
    const size_t numFaces = aiMesh->mNumFaces;
    outData.F.resize(numFaces, 3);
    
    size_t validFaceCount = 0;
    for (size_t i = 0; i < numFaces; ++i) {
        const aiFace& face = aiMesh->mFaces[i];
        
        // Verify face is a triangle (post-processing should guarantee this)
        if (face.mNumIndices != 3) {
            // Skip non-triangular faces (shouldn't happen with aiProcess_Triangulate)
            continue;
        }
        
        outData.F(validFaceCount, 0) = static_cast<int>(face.mIndices[0]);
        outData.F(validFaceCount, 1) = static_cast<int>(face.mIndices[1]);
        outData.F(validFaceCount, 2) = static_cast<int>(face.mIndices[2]);
        ++validFaceCount;
    }
    
    // Resize to actual valid face count
    if (validFaceCount < numFaces) {
        outData.F.conservativeResize(validFaceCount, 3);
    }
    
    if (validFaceCount == 0) {
        lastError_ = "No valid triangular faces after processing";
        throw MeshValidationError(lastError_);
    }
    
    // Compute face normals
    outData.N = computeFaceNormals(outData.V, outData.F);
    
    // Store metadata
    outData.sourcePath = filePath;
    outData.formatHint = ext.substr(1);  // Remove leading '.'
    
    // Validate manifoldness (optional - throws on failure)
    try {
        validateManifold(outData);
    } catch (const NonManifoldMeshError& e) {
        // Log warning but don't fail (some SLM workflows can handle non-manifold)
        lastError_ = "Warning: " + std::string(e.what());
        // Re-throw if strict validation is required
        // throw;
    }
    
    return true;
}

// ==============================================================================
// Manifold Validation
// ==============================================================================

bool MeshIO::validateManifold(const MeshData& data) {
    if (data.isEmpty()) {
        throw MeshValidationError("Cannot validate empty mesh");
    }
    
    // Detect non-manifold edges
    size_t nonManifoldCount = detectNonManifoldEdges(data.F);
    
    if (nonManifoldCount > 0) {
        std::string details = "Mesh has " + std::to_string(nonManifoldCount) +
                             " non-manifold edges. A manifold mesh requires " +
                             "each edge to be shared by exactly 2 faces.";
        throw NonManifoldMeshError(nonManifoldCount, details);
    }
    
    return true;  // Manifold
}

// ==============================================================================
// Format Support Queries
// ==============================================================================

bool MeshIO::isSupportedFormat(const std::string& extension) {
    static const std::unordered_set<std::string> supported = {
        ".stl",   // Stereolithography (Binary/ASCII)
        ".obj",   // Wavefront OBJ
        ".amf",   // Additive Manufacturing Format (XML-based)
        ".3mf",   // 3D Manufacturing Format (ZIP-based)
        ".ply",   // Stanford Polygon (Binary/ASCII)
        ".fbx"    // Autodesk FBX
    };
    
    std::string lowerExt = extension;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
                  [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    return supported.find(lowerExt) != supported.end();
}

std::vector<std::string> MeshIO::supportedFormats() {
    return {".stl", ".obj", ".amf", ".3mf", ".ply", ".fbx"};
}

// ==============================================================================
// Internal: Face Normal Computation
// ==============================================================================

Eigen::MatrixXd MeshIO::computeFaceNormals(const Eigen::MatrixXd& V,
                                           const Eigen::MatrixXi& F) {
    const size_t numFaces = static_cast<size_t>(F.rows());
    Eigen::MatrixXd N(numFaces, 3);
    
    for (size_t i = 0; i < numFaces; ++i) {
        // Get vertex indices
        const int i0 = F(i, 0);
        const int i1 = F(i, 1);
        const int i2 = F(i, 2);
        
        // Get vertex positions
        Eigen::Vector3d v0 = V.row(i0).transpose();
        Eigen::Vector3d v1 = V.row(i1).transpose();
        Eigen::Vector3d v2 = V.row(i2).transpose();
        
        // Compute edge vectors
        Eigen::Vector3d edge1 = v1 - v0;
        Eigen::Vector3d edge2 = v2 - v0;
        
        // Compute cross product (CCW winding)
        Eigen::Vector3d normal = edge1.cross(edge2);
        
        // Normalize (handle zero-length normals for degenerate faces)
        double length = normal.norm();
        if (length > 1e-12) {
            normal /= length;
        } else {
            // Degenerate face: set default normal
            normal = Eigen::Vector3d(0.0, 0.0, 1.0);
        }
        
        N.row(i) = normal.transpose();
    }
    
    return N;
}

// ==============================================================================
// Internal: Non-Manifold Edge Detection
// ==============================================================================

size_t MeshIO::detectNonManifoldEdges(const Eigen::MatrixXi& F) {
    // Build edge-to-face-count map
    std::unordered_map<Edge, int, EdgeHash> edgeCounts;
    
    const size_t numFaces = static_cast<size_t>(F.rows());
    for (size_t i = 0; i < numFaces; ++i) {
        // Extract 3 edges of the triangle
        int v0 = F(i, 0);
        int v1 = F(i, 1);
        int v2 = F(i, 2);
        
        Edge e0(v0, v1);
        Edge e1(v1, v2);
        Edge e2(v2, v0);
        
        // Increment edge counts
        edgeCounts[e0]++;
        edgeCounts[e1]++;
        edgeCounts[e2]++;
    }
    
    // Count edges that are not shared by exactly 2 faces
    size_t nonManifoldCount = 0;
    for (const auto& [edge, count] : edgeCounts) {
        if (count != 2) {
            ++nonManifoldCount;
        }
    }
    
    return nonManifoldCount;
}

// ==============================================================================
// Internal: File Extension Extraction
// ==============================================================================

std::string MeshIO::getFileExtension(const std::string& filePath) {
    std::filesystem::path path(filePath);
    std::string ext = path.extension().string();
    
    // Convert to lowercase
    std::transform(ext.begin(), ext.end(), ext.begin(),
                  [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    return ext;
}

} // namespace IO
} // namespace MarcSLM
