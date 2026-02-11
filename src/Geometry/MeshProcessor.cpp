// ==============================================================================
// MarcSLM - Mesh Processor Implementation
// ==============================================================================
// High-performance 3D mesh slicing engine implementation
// ==============================================================================

#include "MarcSLM/Geometry/MeshProcessor.hpp"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <clipper2/clipper.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace MarcSLM {
namespace Geometry {

// ==============================================================================
// Mesh Loading and Validation
// ==============================================================================

std::unique_ptr<manifold::Manifold>
MeshProcessor::loadMesh(const std::string& filePath) {
    // Create Assimp importer with robust processing flags
    Assimp::Importer importer;

    // Processing flags for robust mesh loading:
    // - Triangulate: Ensure all faces are triangles
    // - JoinIdenticalVertices: Merge duplicate vertices
    // - FixInfacingNormals: Correct inverted normals
    // - RemoveRedundantMaterials: Simplify material handling
    // - FindDegenerates: Remove degenerate triangles
    // - SortByPType: Group faces by primitive type
    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FixInfacingNormals |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_FindDegenerates |
        aiProcess_SortByPType;

    // Load the scene
    const aiScene* scene = importer.ReadFile(filePath, flags);
    if (!scene) {
        throw MeshLoadError(filePath, importer.GetErrorString());
    }

    // Validate scene structure
    if (!scene->HasMeshes()) {
        throw MeshLoadError(filePath, "Scene contains no meshes");
    }

    // For now, process only the first mesh (most common case)
    // TODO: Support multi-mesh scenes with proper merging
    const aiMesh* aiMesh = scene->mMeshes[0];
    if (!aiMesh) {
        throw MeshLoadError(filePath, "First mesh is null");
    }

    // Validate mesh has required data
    if (!aiMesh->HasPositions() || !aiMesh->HasFaces()) {
        throw MeshLoadError(filePath, "Mesh missing positions or faces");
    }

    if (aiMesh->mNumFaces == 0 || aiMesh->mNumVertices < 3) {
        throw MeshLoadError(filePath, "Mesh is empty or degenerate");
    }

    // Convert Assimp mesh to Manifold MeshGL
    manifold::MeshGL manifoldMesh = assimpToManifoldMesh(aiMesh);

    // Create Manifold solid
    auto manifoldPtr = std::make_unique<manifold::Manifold>(manifoldMesh);

    // Validate the manifold
    manifold::Error status = manifoldPtr->Status();
    if (status != manifold::Error::NoError) {
        std::string errorMsg = "Manifold creation failed with status: ";
        switch (status) {
            case manifold::Error::NonFiniteVertex:
                errorMsg += "Non-finite vertex";
                break;
            case manifold::Error::NotManifold:
                errorMsg += "Not manifold";
                break;
            case manifold::Error::VertexOutOfBounds:
                errorMsg += "Vertex out of bounds";
                break;
            case manifold::Error::PropertiesWrongLength:
                errorMsg += "Properties wrong length";
                break;
            case manifold::Error::MissingPositionProperties:
                errorMsg += "Missing position properties";
                break;
            case manifold::Error::MergeVectorsDifferentLengths:
                errorMsg += "Merge vectors different lengths";
                break;
            case manifold::Error::MergeIndexOutOfBounds:
                errorMsg += "Merge index out of bounds";
                break;
            case manifold::Error::TransformWrongLength:
                errorMsg += "Transform wrong length";
                break;
            case manifold::Error::RunIndexWrongLength:
                errorMsg += "Run index wrong length";
                break;
            case manifold::Error::FaceIDWrongLength:
                errorMsg += "Face ID wrong length";
                break;
            default:
                errorMsg += "Unknown error";
                break;
        }
        throw NonManifoldMeshError(errorMsg);
    }

    // Cache bounding box for future queries
    bbox_ = manifoldPtr->BoundingBox();

    // Store the mesh
    mesh_ = std::move(manifoldPtr);

    return std::unique_ptr<manifold::Manifold>(mesh_.get());
}

manifold::Box MeshProcessor::getBoundingBox() const {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded");
    }

    if (!bbox_) {
        bbox_ = mesh_->BoundingBox();
    }

    return *bbox_;
}

std::tuple<size_t, size_t, double> MeshProcessor::getMeshStats() const {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded");
    }

    return {mesh_->NumVert(), mesh_->NumTri(), mesh_->Volume()};
}

// ==============================================================================
// Private Helper: Assimp to Manifold Conversion
// ==============================================================================

manifold::MeshGL MeshProcessor::assimpToManifoldMesh(const aiMesh* mesh) const {
    manifold::MeshGL result;

    // Validate input
    if (!mesh || !mesh->HasPositions() || !mesh->HasFaces()) {
        throw MeshProcessingError("Invalid Assimp mesh for conversion");
    }

    const size_t numVerts = mesh->mNumVertices;
    const size_t numFaces = mesh->mNumFaces;

    // Reserve space for vertices (3 floats per vertex: x,y,z)
    result.vertProperties.reserve(numVerts * 3);

    // Reserve space for triangles (3 indices per triangle)
    result.triVerts.reserve(numFaces * 3);

    // Copy vertex positions
    for (size_t i = 0; i < numVerts; ++i) {
        const aiVector3D& pos = mesh->mVertices[i];
        result.vertProperties.push_back(pos.x);
        result.vertProperties.push_back(pos.y);
        result.vertProperties.push_back(pos.z);
    }

    // Copy triangle indices
    for (size_t i = 0; i < numFaces; ++i) {
        const aiFace& face = mesh->mFaces[i];

        // Ensure it's a triangle (should be due to aiProcess_Triangulate)
        if (face.mNumIndices != 3) {
            throw MeshProcessingError("Non-triangular face found after triangulation");
        }

        result.triVerts.push_back(static_cast<uint32_t>(face.mIndices[0]));
        result.triVerts.push_back(static_cast<uint32_t>(face.mIndices[1]));
        result.triVerts.push_back(static_cast<uint32_t>(face.mIndices[2]));
    }

    // Set number of properties per vertex (just positions: x,y,z)
    result.numProp = 3;

    return result;
}

// ==============================================================================
// Slicing Algorithms
// ==============================================================================

LayerStack MeshProcessor::sliceUniform(float layerThickness) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }

    validateSlicingParams(layerThickness);

    // Get mesh bounds
    const manifold::Box bbox = getBoundingBox();
    const float zMin = bbox.min.z;
    const float zMax = bbox.max.z;

    // Calculate number of layers
    const float totalHeight = zMax - zMin;
    const size_t numLayers = static_cast<size_t>(std::ceil(totalHeight / layerThickness));

    if (numLayers == 0) {
        throw SlicingError("Mesh has zero or negative height");
    }

    // Prepare result vector
    LayerStack layers;
    layers.reserve(numLayers);

    // Parallel slicing using Intel TBB
    // Each iteration processes one layer independently
    tbb::parallel_for(size_t{0}, numLayers, [&](size_t layerIdx) {
        const float zHeight = zMin + layerIdx * layerThickness;
        LayerSlices layer = sliceAtHeight(zHeight);
        layer.layerNumber = static_cast<uint32_t>(layerIdx);
        layer.layerHeight = zHeight;
        layer.layerThickness = layerThickness;

        // Thread-safe: each thread writes to its own index
        layers[layerIdx] = std::move(layer);
    });

    return layers;
}

LayerStack MeshProcessor::sliceAdaptive(float minHeight, float maxHeight, float maxError) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }

    validateAdaptiveParams(minHeight, maxHeight, maxError);

    // Get mesh bounds
    const manifold::Box bbox = getBoundingBox();
    const float zMin = bbox.min.z;
    const float zMax = bbox.max.z;

    LayerStack layers;
    float currentZ = zMin;
    uint32_t layerIdx = 0;

    // Adaptive slicing loop
    while (currentZ < zMax) {
        // Compute adaptive layer height
        const float delta = std::min(0.001f, minHeight * 0.1f);  // Small comparison offset
        const float layerHeight = computeAdaptiveHeight(currentZ, delta, maxError,
                                                       minHeight, maxHeight);

        // Clamp to remaining height
        const float actualHeight = std::min(layerHeight, zMax - currentZ);

        // Slice at this height
        LayerSlices layer = sliceAtHeight(currentZ);
        layer.layerNumber = layerIdx;
        layer.layerHeight = currentZ;
        layer.layerThickness = actualHeight;

        layers.push_back(std::move(layer));

        // Advance to next layer
        currentZ += actualHeight;
        ++layerIdx;

        // Safety check to prevent infinite loops
        if (layerIdx > 100000) {
            throw SlicingError("Adaptive slicing exceeded maximum layer count (100k)");
        }
    }

    return layers;
}

LayerSlices MeshProcessor::sliceAtHeight(float zHeight) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }

    // Slice the manifold at this Z-height
    manifold::Polygons polygons = mesh_->Slice(zHeight);

    // Convert to LayerSlices (currently just outer contours)
    // TODO: Add hatching, infill, and support generation
    return createLayerFromPolygons(polygons, zHeight, 0, 0.0f);
}

LayerStack MeshProcessor::sliceAtHeights(const std::vector<float>& zHeights) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }

    if (zHeights.empty()) {
        return {};
    }

    LayerStack layers;
    layers.reserve(zHeights.size());

    // Parallel slicing for multiple heights
    tbb::parallel_for(size_t{0}, zHeights.size(), [&](size_t i) {
        const float zHeight = zHeights[i];
        LayerSlices layer = sliceAtHeight(zHeight);
        layer.layerNumber = static_cast<uint32_t>(i);
        layer.layerHeight = zHeight;
        layer.layerThickness = 0.0f;  // Unknown for arbitrary heights

        layers[i] = std::move(layer);
    });

    return layers;
}

// ==============================================================================
// Private Helper: Manifold to Marc Conversion
// ==============================================================================

Marc::Core::Slice MeshProcessor::manifoldToMarcSlice(const manifold::Polygons& polygons,
                                                    double zHeight, uint32_t layerIndex) const {
    Marc::Core::Slice result;

    if (polygons.empty()) {
        return result;  // Empty slice
    }

    // Convert manifold polygons to Clipper2 format
    // Manifold Polygons is std::vector<SimplePolygon>
    // SimplePolygon is std::vector<glm::vec2>

    for (const auto& simplePoly : polygons) {
        if (simplePoly.empty()) continue;

        // Convert glm::vec2 points to Clipper2 Point64
        // Scale by 1000 for micron-precision integer math
        Clipper2Lib::Path64 path;
        path.reserve(simplePoly.size());

        for (const glm::vec2& pt : simplePoly) {
            // Convert mm to microns (integer coordinates)
            const int64_t x = static_cast<int64_t>(pt.x * 1000.0);
            const int64_t y = static_cast<int64_t>(pt.y * 1000.0);
            path.emplace_back(x, y);
        }

        // For now, treat all polygons as outer contours
        // TODO: Use Clipper2Lib::PolyTree64 to properly identify contours vs holes
        // based on winding direction and nesting
        if (result.contour.empty()) {
            result.contour = std::move(path);
        } else {
            // Additional polygons treated as holes for now
            result.holes.push_back(std::move(path));
        }
    }

    return result;
}

LayerSlices MeshProcessor::createLayerFromPolygons(const manifold::Polygons& polygons,
                                                  float zHeight, uint32_t layerIndex,
                                                  float layerThickness) {
    LayerSlices layer;
    layer.layerNumber = layerIndex;
    layer.layerHeight = zHeight;
    layer.layerThickness = layerThickness;

    if (polygons.empty()) {
        return layer;  // Empty layer
    }

    // Convert each polygon to a polyline in the layer
    for (const auto& simplePoly : polygons) {
        if (simplePoly.empty()) continue;

        Marc::Polyline polyline;
        polyline.tag.type = Marc::GeometryType::Perimeter;
        polyline.tag.buildStyle = Marc::BuildStyleID::CoreContour_Volume;
        polyline.tag.layerNumber = layerIndex;

        // Convert glm::vec2 to Marc::Point (mm scale)
        polyline.points.reserve(simplePoly.size());
        for (const glm::vec2& pt : simplePoly) {
            polyline.points.emplace_back(static_cast<float>(pt.x),
                                       static_cast<float>(pt.y));
        }

        // Close the polyline if it's not already closed
        if (!polyline.points.empty() &&
            (polyline.points.front().x != polyline.points.back().x ||
             polyline.points.front().y != polyline.points.back().y)) {
            polyline.points.push_back(polyline.points.front());
        }

        layer.polylines.push_back(std::move(polyline));
    }

    return layer;
}

// ==============================================================================
// Adaptive Slicing Helper
// ==============================================================================

float MeshProcessor::computeAdaptiveHeight(float z, float delta, float maxError,
                                          float minHeight, float maxHeight) const {
    // Slice at current Z and Z+delta
    const manifold::Polygons polyZ = mesh_->Slice(z);
    const manifold::Polygons polyZPlus = mesh_->Slice(z + delta);

    // Compute areas
    double areaZ = 0.0;
    for (const auto& poly : polyZ) {
        // Approximate area using shoelace formula
        if (poly.size() >= 3) {
            for (size_t i = 0; i < poly.size(); ++i) {
                const size_t j = (i + 1) % poly.size();
                areaZ += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
            }
        }
    }
    areaZ = std::abs(areaZ) * 0.5;

    double areaZPlus = 0.0;
    for (const auto& poly : polyZPlus) {
        if (poly.size() >= 3) {
            for (size_t i = 0; i < poly.size(); ++i) {
                const size_t j = (i + 1) % poly.size();
                areaZPlus += poly[i].x * poly[j].y - poly[j].x * poly[i].y;
            }
        }
    }
    areaZPlus = std::abs(areaZPlus) * 0.5;

    // Compute relative area change
    const double areaChange = std::abs(areaZPlus - areaZ) /
                             std::max(areaZ, std::numeric_limits<double>::epsilon());

    // If area change is small, we can use larger layer height
    // If area change is large, we need smaller layer height for accuracy
    if (areaChange < maxError) {
        // Low curvature region - can use larger step
        return maxHeight;
    } else {
        // High curvature region - need smaller step
        return minHeight;
    }
}

// ==============================================================================
// Parameter Validation
// ==============================================================================

void MeshProcessor::validateSlicingParams(float layerThickness) const {
    if (layerThickness <= 0.0f) {
        throw SlicingError("Layer thickness must be positive");
    }

    if (layerThickness < 0.001f) {  // 1 micron minimum
        throw SlicingError("Layer thickness too small (< 1 micron)");
    }

    if (layerThickness > 10.0f) {   // 10 mm maximum
        throw SlicingError("Layer thickness too large (> 10 mm)");
    }
}

void MeshProcessor::validateAdaptiveParams(float minHeight, float maxHeight,
                                          float maxError) const {
    if (minHeight <= 0.0f || maxHeight <= 0.0f) {
        throw SlicingError("Layer heights must be positive");
    }

    if (minHeight >= maxHeight) {
        throw SlicingError("Minimum height must be less than maximum height");
    }

    if (maxError < 0.0f || maxError > 1.0f) {
        throw SlicingError("Max error must be in range [0.0, 1.0]");
    }

    if (minHeight < 0.001f) {  // 1 micron minimum
        throw SlicingError("Minimum layer height too small (< 1 micron)");
    }

    if (maxHeight > 10.0f) {   // 10 mm maximum
        throw SlicingError("Maximum layer height too large (> 10 mm)");
    }
}

} // namespace Geometry
} // namespace MarcSLM
