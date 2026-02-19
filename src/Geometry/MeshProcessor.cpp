// ==============================================================================
// MarcSLM - Mesh Processor Implementation
// ==============================================================================
// Uses Assimp for loading and TriMesh (admesh-inspired) for repair + slicing.
// Replaces Manifold dependency with proven Legacy Slic3r algorithms.
// ==============================================================================

#include "MarcSLM/Geometry/MeshProcessor.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>

namespace MarcSLM {
namespace Geometry {

// ==============================================================================
// Mesh Loading (Assimp → TriMesh → Repair)
// ==============================================================================

void MeshProcessor::loadMesh(const std::string& filePath) {
    Assimp::Importer importer;

    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_JoinIdenticalVertices |
        aiProcess_FixInfacingNormals |
        aiProcess_RemoveRedundantMaterials |
        aiProcess_FindDegenerates |
        aiProcess_SortByPType |
        aiProcess_GenSmoothNormals |
        aiProcess_ValidateDataStructure |
        aiProcess_ImproveCacheLocality |
        aiProcess_FindInvalidData;

    const aiScene* scene = importer.ReadFile(filePath, flags);
    if (!scene) {
        throw MeshLoadError(filePath, importer.GetErrorString());
    }

    if (!scene->HasMeshes() || scene->mNumMeshes == 0) {
        throw MeshLoadError(filePath, "Scene contains no meshes");
    }

    // Merge all meshes from the scene into one TriMesh
    // (handles multi-mesh files like OBJ with groups, 3MF, etc.)
    std::vector<float> allVertices;
    std::vector<uint32_t> allIndices;
    uint32_t vertexOffset = 0;

    for (unsigned int mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* aiM = scene->mMeshes[mi];
        if (!aiM || !aiM->HasPositions() || !aiM->HasFaces()) continue;
        if (aiM->mNumFaces == 0 || aiM->mNumVertices < 3) continue;

        // Append vertices
        for (unsigned int vi = 0; vi < aiM->mNumVertices; ++vi) {
            allVertices.push_back(aiM->mVertices[vi].x);
            allVertices.push_back(aiM->mVertices[vi].y);
            allVertices.push_back(aiM->mVertices[vi].z);
        }

        // Append face indices (offset by prior vertex count)
        for (unsigned int fi = 0; fi < aiM->mNumFaces; ++fi) {
            const aiFace& face = aiM->mFaces[fi];
            if (face.mNumIndices != 3) continue;
            allIndices.push_back(vertexOffset + face.mIndices[0]);
            allIndices.push_back(vertexOffset + face.mIndices[1]);
            allIndices.push_back(vertexOffset + face.mIndices[2]);
        }

        vertexOffset += aiM->mNumVertices;
    }

    const size_t numVerts = allVertices.size() / 3;
    const size_t numFaces = allIndices.size() / 3;

    if (numFaces == 0 || numVerts < 3) {
        throw MeshLoadError(filePath, "Mesh is empty or degenerate after processing");
    }

    std::cout << "  Assimp loaded: " << numVerts << " vertices, "
              << numFaces << " faces from " << scene->mNumMeshes << " mesh(es)\n";

    // Build TriMesh from raw arrays
    auto triMesh = std::make_unique<TriMesh>();
    triMesh->buildFromArrays(allVertices.data(), numVerts,
                              allIndices.data(), numFaces);

    // Repair topology (edge matching, degenerate removal, normal fixing)
    triMesh->repair();

    const auto& stats = triMesh->stats();
    std::cout << "  Final mesh: " << triMesh->facetCount() << " facets, "
              << triMesh->vertexCount() << " shared vertices, volume: "
              << stats.volume << " mm³\n";

    if (!triMesh->isManifold()) {
        std::cout << "  ⚠ Mesh is non-manifold but repaired for slicing\n";
    } else {
        std::cout << "  ✓ Mesh is manifold (watertight)\n";
    }

    if (stats.volume <= 0.0f) {
        std::cout << "  ⚠ Warning: Mesh has zero or negative volume\n";
    }

    // Cache bounding box
    bbox_ = triMesh->bbox();

    mesh_ = std::move(triMesh);
}

BBox3f MeshProcessor::getBoundingBox() const {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded");
    }
    if (!bbox_) {
        bbox_ = mesh_->bbox();
    }
    return *bbox_;
}

std::tuple<size_t, size_t, double> MeshProcessor::getMeshStats() const {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded");
    }
    return {mesh_->vertexCount(), mesh_->facetCount(),
            static_cast<double>(mesh_->stats().volume)};
}

// ==============================================================================
// Slicing: Uniform
// ==============================================================================

LayerStack MeshProcessor::sliceUniform(float layerThickness) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }
    validateSlicingParams(layerThickness);

    const BBox3f bb = getBoundingBox();
    const float zMin = bb.min.z;
    const float zMax = bb.max.z;
    const float totalHeight = zMax - zMin;
    const size_t numLayers = static_cast<size_t>(std::ceil(totalHeight / layerThickness));

    if (numLayers == 0) {
        throw SlicingError("Mesh has zero or negative height");
    }

    // Build Z-height vector (slice at midpoint of each layer)
    std::vector<float> zHeights(numLayers);
    for (size_t i = 0; i < numLayers; ++i) {
        float lo = zMin + i * layerThickness;
        float hi = std::min(lo + layerThickness, zMax);
        zHeights[i] = 0.5f * (lo + hi);
    }

    // Slice all Z-heights at once (efficient: single pass over facets)
    std::vector<ExPolygons2i> exPolygonsByLayer = mesh_->slice(zHeights);

    // Convert to Marc::Layer format
    LayerStack layers(numLayers);
    for (size_t i = 0; i < numLayers; ++i) {
        float zHeight = zMin + i * layerThickness;
        layers[i] = createLayerFromExPolygons(
            exPolygonsByLayer[i], zHeight,
            static_cast<uint32_t>(i), layerThickness);
    }

    return layers;
}

// ==============================================================================
// Slicing: Adaptive
// ==============================================================================

LayerStack MeshProcessor::sliceAdaptive(float minHeight, float maxHeight,
                                         float maxError) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }
    validateAdaptiveParams(minHeight, maxHeight, maxError);

    const BBox3f bb = getBoundingBox();
    const float zMin = bb.min.z;
    const float zMax = bb.max.z;

    // First pass: determine layer Z-heights adaptively
    std::vector<float> zHeights;
    std::vector<float> thicknesses;
    float currentZ = zMin;
    while (currentZ < zMax) {
        float delta = std::min(0.001f, minHeight * 0.1f);
        float h = computeAdaptiveHeight(currentZ, delta, maxError,
                                         minHeight, maxHeight);
        h = std::min(h, zMax - currentZ);
        float sliceZ = currentZ + h * 0.5f;
        zHeights.push_back(sliceZ);
        thicknesses.push_back(h);
        currentZ += h;

        if (zHeights.size() > 100000) {
            throw SlicingError("Adaptive slicing exceeded maximum layer count (100k)");
        }
    }

    if (zHeights.empty()) return {};

    // Second pass: slice all at once
    auto exPolygonsByLayer = mesh_->slice(zHeights);

    LayerStack layers(zHeights.size());
    float printZ = zMin;
    for (size_t i = 0; i < zHeights.size(); ++i) {
        layers[i] = createLayerFromExPolygons(
            exPolygonsByLayer[i], printZ,
            static_cast<uint32_t>(i), thicknesses[i]);
        printZ += thicknesses[i];
    }

    return layers;
}

// ==============================================================================
// Slicing: Single Height
// ==============================================================================

LayerSlices MeshProcessor::sliceAtHeight(float zHeight) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }

    std::vector<float> zVec = {zHeight};
    auto result = mesh_->slice(zVec);
    if (result.empty()) return LayerSlices{};
    return createLayerFromExPolygons(result[0], zHeight, 0, 0.0f);
}

LayerStack MeshProcessor::sliceAtHeights(const std::vector<float>& zHeights) {
    if (!hasValidMesh()) {
        throw MeshProcessingError("No valid mesh loaded for slicing");
    }
    if (zHeights.empty()) return {};

    auto exPolygonsByLayer = mesh_->slice(zHeights);

    LayerStack layers(zHeights.size());
    for (size_t i = 0; i < zHeights.size(); ++i) {
        layers[i] = createLayerFromExPolygons(
            exPolygonsByLayer[i], zHeights[i],
            static_cast<uint32_t>(i), 0.0f);
    }
    return layers;
}

// ==============================================================================
// Conversion: ExPolygon2i → Marc::Layer
// ==============================================================================

LayerSlices MeshProcessor::createLayerFromExPolygons(
    const ExPolygons2i& exPolygons,
    float zHeight, uint32_t layerIndex,
    float layerThickness) const {

    LayerSlices layer;
    layer.layerNumber = layerIndex;
    layer.layerHeight = zHeight;
    layer.layerThickness = layerThickness;

    if (exPolygons.empty()) return layer;

    for (const auto& exPoly : exPolygons) {
        // Convert contour to Marc::Polyline (float mm)
        {
            Marc::Polyline polyline;
            polyline.tag.type = Marc::GeometryType::Perimeter;
            polyline.tag.buildStyle = Marc::BuildStyleID::CoreContour_Volume;
            polyline.tag.layerNumber = layerIndex;

            polyline.points.reserve(exPoly.contour.size() + 1);
            for (const auto& pt : exPoly.contour) {
                float x = static_cast<float>(pt.x * MESH_SCALING_FACTOR);
                float y = static_cast<float>(pt.y * MESH_SCALING_FACTOR);
                polyline.points.emplace_back(x, y);
            }
            // Close the contour
            if (!polyline.points.empty()) {
                polyline.points.push_back(polyline.points.front());
            }

            layer.polylines.push_back(std::move(polyline));
        }

        // Convert holes to Marc::Polyline
        for (const auto& hole : exPoly.holes) {
            Marc::Polyline holeLine;
            holeLine.tag.type = Marc::GeometryType::Perimeter;
            holeLine.tag.buildStyle = Marc::BuildStyleID::CoreContour_Volume;
            holeLine.tag.layerNumber = layerIndex;

            holeLine.points.reserve(hole.size() + 1);
            for (const auto& pt : hole) {
                float x = static_cast<float>(pt.x * MESH_SCALING_FACTOR);
                float y = static_cast<float>(pt.y * MESH_SCALING_FACTOR);
                holeLine.points.emplace_back(x, y);
            }
            if (!holeLine.points.empty()) {
                holeLine.points.push_back(holeLine.points.front());
            }

            layer.polylines.push_back(std::move(holeLine));
        }
    }

    return layer;
}

// ==============================================================================
// Adaptive Height Computation
// ==============================================================================

float MeshProcessor::computeAdaptiveHeight(float z, float delta, float maxError,
                                            float minHeight, float maxHeight) const {
    // Slice at z and z+delta to estimate cross-section area change
    std::vector<float> zVec = {z, z + delta};
    auto result = mesh_->slice(zVec);

    auto computeArea = [](const ExPolygons2i& exps) -> double {
        double total = 0.0;
        for (const auto& ex : exps) {
            double contourArea = 0.0;
            const auto& c = ex.contour;
            for (size_t i = 0; i < c.size(); ++i) {
                size_t j = (i + 1) % c.size();
                contourArea += static_cast<double>(c[i].x) * c[j].y;
                contourArea -= static_cast<double>(c[j].x) * c[i].y;
            }
            total += std::abs(contourArea) * 0.5;
            for (const auto& h : ex.holes) {
                double holeArea = 0.0;
                for (size_t i = 0; i < h.size(); ++i) {
                    size_t j = (i + 1) % h.size();
                    holeArea += static_cast<double>(h[i].x) * h[j].y;
                    holeArea -= static_cast<double>(h[j].x) * h[i].y;
                }
                total -= std::abs(holeArea) * 0.5;
            }
        }
        return total;
    };

    double areaZ = (result.size() > 0) ? computeArea(result[0]) : 0.0;
    double areaZp = (result.size() > 1) ? computeArea(result[1]) : 0.0;
    double denom = std::max(areaZ, std::numeric_limits<double>::epsilon());
    double areaChange = std::abs(areaZp - areaZ) / denom;

    return (areaChange < maxError) ? maxHeight : minHeight;
}

// ==============================================================================
// Parameter Validation
// ==============================================================================

void MeshProcessor::validateSlicingParams(float layerThickness) const {
    if (layerThickness <= 0.0f)
        throw SlicingError("Layer thickness must be positive");
    if (layerThickness < 0.001f)
        throw SlicingError("Layer thickness too small (< 1 micron)");
    if (layerThickness > 10.0f)
        throw SlicingError("Layer thickness too large (> 10 mm)");
}

void MeshProcessor::validateAdaptiveParams(float minHeight, float maxHeight,
                                            float maxError) const {
    if (minHeight <= 0.0f || maxHeight <= 0.0f)
        throw SlicingError("Layer heights must be positive");
    if (minHeight >= maxHeight)
        throw SlicingError("Minimum height must be less than maximum height");
    if (maxError < 0.0f || maxError > 1.0f)
        throw SlicingError("Max error must be in range [0.0, 1.0]");
    if (minHeight < 0.001f)
        throw SlicingError("Minimum layer height too small (< 1 micron)");
    if (maxHeight > 10.0f)
        throw SlicingError("Maximum layer height too large (> 10 mm)");
}

} // namespace Geometry
} // namespace MarcSLM
