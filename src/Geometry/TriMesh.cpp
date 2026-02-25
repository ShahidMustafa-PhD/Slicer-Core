// ==============================================================================
// MarcSLM - TriMesh Implementation
// ==============================================================================
// Ported from Legacy Slic3r::TriangleMesh + TriangleMeshSlicer<Z>.
// Provides topology repair and Z-plane slicing without Manifold dependency.
// ==============================================================================

#include "MarcSLM/Geometry/TriMesh.hpp"

#include <clipper2/clipper.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>

namespace MarcSLM {
namespace Geometry {

// ==============================================================================
// Construction / Destruction
// ==============================================================================

TriMesh::TriMesh() = default;
TriMesh::~TriMesh() = default;

TriMesh::TriMesh(const TriMesh& other)
    : facets_(other.facets_)
    , neighbors_(other.neighbors_)
    , vertexIndices_(other.vertexIndices_)
    , sharedVertices_(other.sharedVertices_)
    , bbox_(other.bbox_)
    , stats_(other.stats_)
    , repaired_(other.repaired_) {}

TriMesh& TriMesh::operator=(const TriMesh& other) {
    if (this != &other) {
        facets_          = other.facets_;
        neighbors_       = other.neighbors_;
        vertexIndices_   = other.vertexIndices_;
        sharedVertices_  = other.sharedVertices_;
        bbox_            = other.bbox_;
        stats_           = other.stats_;
        repaired_        = other.repaired_;
    }
    return *this;
}

TriMesh::TriMesh(TriMesh&& other) noexcept
    : facets_(std::move(other.facets_))
    , neighbors_(std::move(other.neighbors_))
    , vertexIndices_(std::move(other.vertexIndices_))
    , sharedVertices_(std::move(other.sharedVertices_))
    , bbox_(other.bbox_)
    , stats_(other.stats_)
    , repaired_(other.repaired_) {
    other.repaired_ = false;
}

TriMesh& TriMesh::operator=(TriMesh&& other) noexcept {
    if (this != &other) {
        facets_          = std::move(other.facets_);
        neighbors_       = std::move(other.neighbors_);
        vertexIndices_   = std::move(other.vertexIndices_);
        sharedVertices_  = std::move(other.sharedVertices_);
        bbox_            = other.bbox_;
        stats_           = other.stats_;
        repaired_        = other.repaired_;
        other.repaired_  = false;
    }
    return *this;
}

// ==============================================================================
// Build from Assimp raw arrays
// ==============================================================================

void TriMesh::buildFromArrays(const float* vertices, size_t numVerts,
                               const uint32_t* indices, size_t numFaces) {
    facets_.clear();
    facets_.resize(numFaces);
    neighbors_.clear();
    neighbors_.resize(numFaces);
    vertexIndices_.clear();
    sharedVertices_.clear();
    repaired_ = false;

    for (size_t i = 0; i < numFaces; ++i) {
        Facet& f = facets_[i];
        const uint32_t i0 = indices[i * 3 + 0];
        const uint32_t i1 = indices[i * 3 + 1];
        const uint32_t i2 = indices[i * 3 + 2];

        f.vertex[0] = {vertices[i0 * 3], vertices[i0 * 3 + 1], vertices[i0 * 3 + 2]};
        f.vertex[1] = {vertices[i1 * 3], vertices[i1 * 3 + 1], vertices[i1 * 3 + 2]};
        f.vertex[2] = {vertices[i2 * 3], vertices[i2 * 3 + 1], vertices[i2 * 3 + 2]};

        // Compute face normal via cross product
        float ax = f.vertex[1].x - f.vertex[0].x;
        float ay = f.vertex[1].y - f.vertex[0].y;
        float az = f.vertex[1].z - f.vertex[0].z;
        float bx = f.vertex[2].x - f.vertex[0].x;
        float by = f.vertex[2].y - f.vertex[0].y;
        float bz = f.vertex[2].z - f.vertex[0].z;
        f.normal.x = ay * bz - az * by;
        f.normal.y = az * bx - ax * bz;
        f.normal.z = ax * by - ay * bx;
        float len = std::sqrt(f.normal.x * f.normal.x +
                              f.normal.y * f.normal.y +
                              f.normal.z * f.normal.z);
        if (len > 0.0f) {
            f.normal.x /= len;
            f.normal.y /= len;
            f.normal.z /= len;
        }
    }

    computeBBox();
    stats_.numFacets = numFaces;
}

// ==============================================================================
// Bounding Box
// ==============================================================================

void TriMesh::computeBBox() {
    bbox_ = BBox3f{};
    for (const auto& f : facets_) {
        for (int j = 0; j < 3; ++j) {
            bbox_.merge(f.vertex[j]);
        }
    }
    float dx = bbox_.sizeX(), dy = bbox_.sizeY(), dz = bbox_.sizeZ();
    stats_.boundingDiameter = std::sqrt(dx * dx + dy * dy + dz * dz);
}

// ==============================================================================
// Repair (ported from admesh / Legacy TriangleMesh::repair)
// ==============================================================================

void TriMesh::repair() {
    if (repaired_) return;
    if (facets_.empty()) return;

    std::cout << "  Repairing mesh topology...\n";

    // Step 1: Generate shared vertices by welding identical positions
    generateSharedVertices();

    // Step 2: Build neighbor topology from shared vertices
    checkTopologyExact();

    // Step 3: Try to connect nearby vertices for remaining unmatched edges
    if (stats_.connectedFacets3Edge < stats_.numFacets) {
        float tolerance = stats_.shortestEdge;
        float increment = stats_.boundingDiameter / 10000.0f;
        for (int iter = 0; iter < 5; ++iter) {
            if (stats_.connectedFacets3Edge >= stats_.numFacets) break;
            checkTopologyNearby(tolerance);
            tolerance += increment;
        }
    }

    // Step 4: Remove degenerate facets (zero area)
    removeDegenerate();

    // Step 5: Fix normals direction (ensure consistent winding)
    fixNormals();

    // Step 6: Calculate volume (and reverse normals if negative)
    calculateVolume();

    computeBBox();

    std::cout << "  Repair complete: " << stats_.numFacets << " facets, "
              << stats_.numSharedVertices << " shared vertices\n";
    if (neededRepair()) {
        std::cout << "  Repairs applied: "
                  << stats_.edgesFixed << " edges fixed, "
                  << stats_.facetsRemoved << " facets removed, "
                  << stats_.facetsReversed << " facets reversed, "
                  << stats_.normalsFixed << " normals fixed\n";
    }

    repaired_ = true;
}

bool TriMesh::isManifold() const {
    return stats_.connectedFacets3Edge == stats_.numFacets;
}

bool TriMesh::neededRepair() const {
    return stats_.degenerateFacets > 0 ||
           stats_.edgesFixed > 0 ||
           stats_.facetsRemoved > 0 ||
           stats_.facetsAdded > 0 ||
           stats_.facetsReversed > 0 ||
           stats_.normalsFixed > 0;
}

float TriMesh::volume() {
    if (stats_.volume == 0.0f && !facets_.empty()) {
        calculateVolume();
    }
    return stats_.volume;
}

// ==============================================================================
// Shared Vertex Generation (vertex welding)
// ==============================================================================

void TriMesh::generateSharedVertices() {
    // Build a spatial hash map to weld identical (or nearly-identical) vertices
    struct VertexHash {
        size_t operator()(const std::tuple<int, int, int>& k) const {
            auto h1 = std::hash<int>{}(std::get<0>(k));
            auto h2 = std::hash<int>{}(std::get<1>(k));
            auto h3 = std::hash<int>{}(std::get<2>(k));
            return h1 ^ (h2 << 11) ^ (h3 << 22);
        }
    };

    // Quantize vertices to micron resolution for welding
    const float quantize = 10000.0f;  // 0.1 micron resolution
    std::unordered_map<std::tuple<int, int, int>, int, VertexHash> vertexMap;
    sharedVertices_.clear();
    vertexIndices_.clear();
    vertexIndices_.resize(facets_.size());

    for (size_t fi = 0; fi < facets_.size(); ++fi) {
        for (int vi = 0; vi < 3; ++vi) {
            const Vertex3f& v = facets_[fi].vertex[vi];
            auto key = std::make_tuple(
                static_cast<int>(std::round(v.x * quantize)),
                static_cast<int>(std::round(v.y * quantize)),
                static_cast<int>(std::round(v.z * quantize))
            );

            auto it = vertexMap.find(key);
            if (it != vertexMap.end()) {
                vertexIndices_[fi].vertex[vi] = it->second;
            } else {
                int idx = static_cast<int>(sharedVertices_.size());
                vertexMap[key] = idx;
                sharedVertices_.push_back(v);
                vertexIndices_[fi].vertex[vi] = idx;
            }
        }
    }

    stats_.numSharedVertices = sharedVertices_.size();
}

// ==============================================================================
// Topology Checking (exact edge matching)
// ==============================================================================

void TriMesh::checkTopologyExact() {
    // Build edge → facet map. An edge is an ordered pair of shared vertex indices.
    using Edge = std::pair<int, int>;
    std::unordered_multimap<uint64_t, std::pair<int, int>> edgeMap;  // hash → (facetIdx, edgeIdx)

    auto edgeKey = [](int a, int b) -> uint64_t {
        int lo = std::min(a, b), hi = std::max(a, b);
        return (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
    };

    neighbors_.clear();
    neighbors_.resize(facets_.size());

    for (size_t fi = 0; fi < facets_.size(); ++fi) {
        for (int ei = 0; ei < 3; ++ei) {
            int a = vertexIndices_[fi].vertex[ei];
            int b = vertexIndices_[fi].vertex[(ei + 1) % 3];
            uint64_t key = edgeKey(a, b);
            edgeMap.emplace(key, std::make_pair(static_cast<int>(fi), ei));
        }
    }

    // For each edge, find pairs of facets that share it
    stats_.connectedFacets1Edge = 0;
    stats_.connectedFacets2Edge = 0;
    stats_.connectedFacets3Edge = 0;

    for (size_t fi = 0; fi < facets_.size(); ++fi) {
        int connected = 0;
        for (int ei = 0; ei < 3; ++ei) {
            if (neighbors_[fi].neighbor[ei] != -1) {
                ++connected;
                continue;
            }

            int a = vertexIndices_[fi].vertex[ei];
            int b = vertexIndices_[fi].vertex[(ei + 1) % 3];
            uint64_t key = edgeKey(a, b);

            auto range = edgeMap.equal_range(key);
            for (auto it = range.first; it != range.second; ++it) {
                int otherFi = it->second.first;
                int otherEi = it->second.second;
                if (otherFi == static_cast<int>(fi)) continue;

                // Check that the edge is shared in opposite direction (proper neighbor)
                int otherA = vertexIndices_[otherFi].vertex[otherEi];
                int otherB = vertexIndices_[otherFi].vertex[(otherEi + 1) % 3];
                if ((a == otherB && b == otherA) || (a == otherA && b == otherB)) {
                    neighbors_[fi].neighbor[ei] = otherFi;
                    neighbors_[otherFi].neighbor[otherEi] = static_cast<int>(fi);
                    ++connected;
                    break;
                }
            }
            if (neighbors_[fi].neighbor[ei] == -1) {
                // Still unconnected
            } else {
                ++connected;
            }
        }

        // Update connectivity stats
        if (connected >= 1) ++stats_.connectedFacets1Edge;
        if (connected >= 2) ++stats_.connectedFacets2Edge;
        if (connected >= 3) ++stats_.connectedFacets3Edge;
    }

    // Compute shortest edge length for nearby-tolerance
    stats_.shortestEdge = std::numeric_limits<float>::max();
    for (const auto& f : facets_) {
        for (int ei = 0; ei < 3; ++ei) {
            const Vertex3f& a = f.vertex[ei];
            const Vertex3f& b = f.vertex[(ei + 1) % 3];
            float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
            float len = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (len > 0.0f && len < stats_.shortestEdge)
                stats_.shortestEdge = len;
        }
    }
}

void TriMesh::checkTopologyNearby(float /*tolerance*/) {
    // For now, the exact matching is sufficient for most industrial STL files.
    // The vertex welding in generateSharedVertices() already handles the common case.
    // TODO: Implement tolerance-based edge matching for badly exported STL files.
}

// ==============================================================================
// Degenerate Facet Removal
// ==============================================================================

void TriMesh::removeDegenerate() {
    size_t removed = 0;
    auto it = facets_.begin();
    size_t idx = 0;
    while (it != facets_.end()) {
        const Facet& f = *it;
        // Check for zero-area triangle (two or more vertices at same position)
        bool degenerate = false;
        for (int i = 0; i < 3 && !degenerate; ++i) {
            int j = (i + 1) % 3;
            float dx = f.vertex[i].x - f.vertex[j].x;
            float dy = f.vertex[i].y - f.vertex[j].y;
            float dz = f.vertex[i].z - f.vertex[j].z;
            if (dx * dx + dy * dy + dz * dz < 1e-14f)
                degenerate = true;
        }
        if (degenerate) {
            it = facets_.erase(it);
            if (idx < neighbors_.size()) neighbors_.erase(neighbors_.begin() + idx);
            if (idx < vertexIndices_.size()) vertexIndices_.erase(vertexIndices_.begin() + idx);
            ++removed;
        } else {
            ++it;
            ++idx;
        }
    }
    stats_.degenerateFacets = removed;
    stats_.facetsRemoved += removed;
    stats_.numFacets = facets_.size();
}

// ==============================================================================
// Normal Fixing
// ==============================================================================

void TriMesh::fixNormals() {
    size_t fixed = 0;
    for (auto& f : facets_) {
        // Recompute normal from vertex positions
        float ax = f.vertex[1].x - f.vertex[0].x;
        float ay = f.vertex[1].y - f.vertex[0].y;
        float az = f.vertex[1].z - f.vertex[0].z;
        float bx = f.vertex[2].x - f.vertex[0].x;
        float by = f.vertex[2].y - f.vertex[0].y;
        float bz = f.vertex[2].z - f.vertex[0].z;
        float nx = ay * bz - az * by;
        float ny = az * bx - ax * bz;
        float nz = ax * by - ay * bx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) {
            nx /= len; ny /= len; nz /= len;
            // Check if stored normal is way off
            float dot = f.normal.x * nx + f.normal.y * ny + f.normal.z * nz;
            if (dot < 0.0f) {
                // Normal is inverted - reverse it
                f.normal = {nx, ny, nz};
                ++fixed;
            } else if (std::abs(f.normal.x) < 1e-6f &&
                       std::abs(f.normal.y) < 1e-6f &&
                       std::abs(f.normal.z) < 1e-6f) {
                // Normal was zero - set it
                f.normal = {nx, ny, nz};
                ++fixed;
            }
        }
    }
    stats_.normalsFixed = fixed;
}

void TriMesh::fillHoles() {
    // Hole filling is complex. For SLM slicing, we rely on the slicer's
    // ability to handle open contours, which the loop-chaining algorithm does.
}

// ==============================================================================
// Volume Calculation
// ==============================================================================

void TriMesh::calculateVolume() {
    // Signed volume using divergence theorem
    double vol = 0.0;
    for (const auto& f : facets_) {
        double v321 = f.vertex[2].x * f.vertex[1].y * f.vertex[0].z;
        double v231 = f.vertex[1].x * f.vertex[2].y * f.vertex[0].z;
        double v312 = f.vertex[2].x * f.vertex[0].y * f.vertex[1].z;
        double v132 = f.vertex[0].x * f.vertex[2].y * f.vertex[1].z;
        double v213 = f.vertex[1].x * f.vertex[0].y * f.vertex[2].z;
        double v123 = f.vertex[0].x * f.vertex[1].y * f.vertex[2].z;
        vol += (-v321 + v231 + v312 - v132 - v213 + v123);
    }
    stats_.volume = static_cast<float>(vol / 6.0);

    // If volume is negative, normals are inverted - flip them all
    if (stats_.volume < 0.0f) {
        for (auto& f : facets_) {
            std::swap(f.vertex[0], f.vertex[1]);
            f.normal.x = -f.normal.x;
            f.normal.y = -f.normal.y;
            f.normal.z = -f.normal.z;
        }
        stats_.volume = -stats_.volume;
        stats_.facetsReversed = facets_.size();
        // Regenerate shared vertices after flip
        generateSharedVertices();
    }
}

// ==============================================================================
// Geometric Transforms (ported from Legacy TriangleMesh)
// ==============================================================================

void TriMesh::translate(float dx, float dy, float dz) {
    for (auto& f : facets_) {
        for (int j = 0; j < 3; ++j) {
            f.vertex[j].x += dx;
            f.vertex[j].y += dy;
            f.vertex[j].z += dz;
        }
    }
    for (auto& v : sharedVertices_) {
        v.x += dx;
        v.y += dy;
        v.z += dz;
    }
    if (bbox_.valid()) {
        bbox_.min.x += dx; bbox_.min.y += dy; bbox_.min.z += dz;
        bbox_.max.x += dx; bbox_.max.y += dy; bbox_.max.z += dz;
    }
}

void TriMesh::scale(float factor) {
    scale(factor, factor, factor);
}

void TriMesh::scale(float sx, float sy, float sz) {
    for (auto& f : facets_) {
        for (int j = 0; j < 3; ++j) {
            f.vertex[j].x *= sx;
            f.vertex[j].y *= sy;
            f.vertex[j].z *= sz;
        }
        // Recompute normal after non-uniform scale
        float ax = f.vertex[1].x - f.vertex[0].x;
        float ay = f.vertex[1].y - f.vertex[0].y;
        float az = f.vertex[1].z - f.vertex[0].z;
        float bx = f.vertex[2].x - f.vertex[0].x;
        float by = f.vertex[2].y - f.vertex[0].y;
        float bz = f.vertex[2].z - f.vertex[0].z;
        float nx = ay * bz - az * by;
        float ny = az * bx - ax * bz;
        float nz = ax * by - ay * bx;
        float len = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (len > 0.0f) {
            f.normal = {nx / len, ny / len, nz / len};
        }
    }
    for (auto& v : sharedVertices_) {
        v.x *= sx; v.y *= sy; v.z *= sz;
    }
    computeBBox();
}

void TriMesh::rotateX(float angleRad) {
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    auto rotVert = [c, s](Vertex3f& v) {
        float y = v.y * c - v.z * s;
        float z = v.y * s + v.z * c;
        v.y = y; v.z = z;
    };
    for (auto& f : facets_) {
        for (int j = 0; j < 3; ++j) rotVert(f.vertex[j]);
        rotVert(f.normal);
    }
    for (auto& v : sharedVertices_) rotVert(v);
    computeBBox();
}

void TriMesh::rotateY(float angleRad) {
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    auto rotVert = [c, s](Vertex3f& v) {
        float x =  v.x * c + v.z * s;
        float z = -v.x * s + v.z * c;
        v.x = x; v.z = z;
    };
    for (auto& f : facets_) {
        for (int j = 0; j < 3; ++j) rotVert(f.vertex[j]);
        rotVert(f.normal);
    }
    for (auto& v : sharedVertices_) rotVert(v);
    computeBBox();
}

void TriMesh::rotateZ(float angleRad) {
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    auto rotVert = [c, s](Vertex3f& v) {
        float x = v.x * c - v.y * s;
        float y = v.x * s + v.y * c;
        v.x = x; v.y = y;
    };
    for (auto& f : facets_) {
        for (int j = 0; j < 3; ++j) rotVert(f.vertex[j]);
        rotVert(f.normal);
    }
    for (auto& v : sharedVertices_) rotVert(v);
    computeBBox();
}

void TriMesh::alignToGround() {
    if (facets_.empty()) return;
    computeBBox();
    translate(0.0f, 0.0f, -bbox_.min.z);
}

void TriMesh::centerXY(float cx, float cy) {
    if (facets_.empty()) return;
    computeBBox();
    float midX = (bbox_.min.x + bbox_.max.x) * 0.5f;
    float midY = (bbox_.min.y + bbox_.max.y) * 0.5f;
    translate(cx - midX, cy - midY, 0.0f);
}

void TriMesh::merge(const TriMesh& other) {
    facets_.insert(facets_.end(), other.facets_.begin(), other.facets_.end());
    // Topology is now invalid — neighbors, shared vertices, etc. are stale
    neighbors_.clear();
    vertexIndices_.clear();
    sharedVertices_.clear();
    repaired_ = false;
    stats_ = MeshStats{};
    stats_.numFacets = facets_.size();
    computeBBox();
}

// ==============================================================================
// Slicing: Build Edge-Facet Tables
// ==============================================================================

std::vector<std::vector<int>> TriMesh::buildFacetsEdges() const {
    // Each facet has 3 edges. We assign a unique edge index to each
    // directed edge pair, matching the Legacy TriangleMeshSlicer approach.
    using EdgePair = std::pair<int, int>;
    std::map<EdgePair, int> edgeMap;
    std::vector<std::vector<int>> facetsEdges(facets_.size());

    for (size_t fi = 0; fi < facets_.size(); ++fi) {
        facetsEdges[fi].resize(3);
        for (int ei = 0; ei < 3; ++ei) {
            int a = vertexIndices_[fi].vertex[ei];
            int b = vertexIndices_[fi].vertex[(ei + 1) % 3];

            int edgeIdx;
            // Look for reverse edge first
            auto it = edgeMap.find({b, a});
            if (it != edgeMap.end()) {
                edgeIdx = it->second;
            } else {
                // Look for same-direction edge (non-manifold case)
                it = edgeMap.find({a, b});
                if (it != edgeMap.end()) {
                    edgeIdx = it->second;
                } else {
                    edgeIdx = static_cast<int>(edgeMap.size());
                    edgeMap[{a, b}] = edgeIdx;
                }
            }
            facetsEdges[fi][ei] = edgeIdx;
        }
    }
    return facetsEdges;
}

// ==============================================================================
// Slicing: Main Entry Point
// ==============================================================================

std::vector<ExPolygons2i> TriMesh::slice(const std::vector<float>& zHeights) {
    if (!repaired_) repair();
    if (facets_.empty() || zHeights.empty()) return {};

    // Build edge tables
    auto facetsEdges = buildFacetsEdges();

    // Scale shared vertices (matching Legacy: divide by SCALING_FACTOR)
    std::vector<Vertex3f> scaledShared(sharedVertices_.size());
    for (size_t i = 0; i < sharedVertices_.size(); ++i) {
        scaledShared[i].x = static_cast<float>(sharedVertices_[i].x / MESH_SCALING_FACTOR);
        scaledShared[i].y = static_cast<float>(sharedVertices_[i].y / MESH_SCALING_FACTOR);
        scaledShared[i].z = static_cast<float>(sharedVertices_[i].z / MESH_SCALING_FACTOR);
    }

    // Generate intersection lines for each Z-plane.
    // Sort zHeights to enable binary-search range selection per facet.
    std::vector<ExPolygons2i> result(zHeights.size());

    // Use a sorted copy for the binary search, but remember the original order.
    // (zHeights is assumed pre-sorted by callers — verify here.)
    std::vector<size_t> sortedIdx(zHeights.size());
    std::iota(sortedIdx.begin(), sortedIdx.end(), 0);
    std::vector<float> sortedZ = zHeights;
    // If already sorted (normal case), this is a no-op.
    if (!std::is_sorted(sortedZ.begin(), sortedZ.end())) {
        std::sort(sortedIdx.begin(), sortedIdx.end(),
                  [&](size_t a, size_t b){ return zHeights[a] < zHeights[b]; });
        std::sort(sortedZ.begin(), sortedZ.end());
    }

    std::vector<IntersectionLines> allLines(zHeights.size());

    for (size_t fi = 0; fi < facets_.size(); ++fi) {
        const Facet& facet = facets_[fi];

        // Get vertex Z values in mm from the facet (raw float storage)
        const float vz0 = facet.vertex[0].z;
        const float vz1 = facet.vertex[1].z;
        const float vz2 = facet.vertex[2].z;
        float minZ = std::min({vz0, vz1, vz2});
        float maxZ = std::max({vz0, vz1, vz2});

        // Find Z-planes in sortedZ that could intersect this facet.
        // Include planes at exactly minZ or maxZ (for top/bottom/horizontal edges).
        auto lo = std::lower_bound(sortedZ.begin(), sortedZ.end(), minZ);
        auto hi = std::upper_bound(sortedZ.begin(), sortedZ.end(), maxZ);

        for (auto it = lo; it != hi; ++it) {
            size_t layerIdx = sortedIdx[it - sortedZ.begin()];
            float sliceZ = *it;
            float scaledZ = static_cast<float>(sliceZ / MESH_SCALING_FACTOR);
            sliceFacet(scaledZ, facet, static_cast<int>(fi), minZ, maxZ,
                       facetsEdges, scaledShared.data(), allLines[layerIdx]);
        }
    }

    // Build polygon loops from intersection lines, then ExPolygons
    for (size_t li = 0; li < zHeights.size(); ++li) {
        Polygons2i loops;
        makeLoops(allLines[li], loops);
        makeExPolygons(loops, result[li]);
    }

    return result;
}

// ==============================================================================
// Slicing: Facet-Plane Intersection (ported from Legacy slice_facet)
// ==============================================================================

void TriMesh::sliceFacet(float scaledZ, const Facet& facet, int facetIdx,
                          float minZ, float maxZ,
                          const std::vector<std::vector<int>>& facetsEdges,
                          const Vertex3f* scaledShared,
                          IntersectionLines& lines) const {
    std::vector<IntersectionPoint> points;
    std::vector<size_t> pointsOnLayer;
    bool foundHorizontalEdge = false;

    // Reorder vertices so the one with lowest Z is first
    int startVert = 0;
    if (facet.vertex[1].z == minZ) startVert = 1;
    else if (facet.vertex[2].z == minZ) startVert = 2;

    float sliceZ = static_cast<float>(scaledZ * MESH_SCALING_FACTOR);  // back to mm for comparison

    for (int j = startVert; (j - startVert) < 3; ++j) {
        int ei = j % 3;
        int edgeId = (facetIdx < static_cast<int>(facetsEdges.size())) ?
                     facetsEdges[facetIdx][ei] : -1;
        int aIdx = vertexIndices_[facetIdx].vertex[ei];
        int bIdx = vertexIndices_[facetIdx].vertex[(ei + 1) % 3];
        const Vertex3f& a = scaledShared[aIdx];
        const Vertex3f& b = scaledShared[bIdx];
        float aZ = static_cast<float>(a.z * MESH_SCALING_FACTOR);
        float bZ = static_cast<float>(b.z * MESH_SCALING_FACTOR);

        if (aZ == sliceZ && bZ == sliceZ) {
            // Horizontal edge on this layer
            IntersectionLine line;
            if (minZ == maxZ) {
                line.edgeType = IntersectionLine::Horizontal;
                if (facet.normal.z < 0) {
                    // Bottom horizontal facet - reverse
                    line.a = {static_cast<int64_t>(b.x), static_cast<int64_t>(b.y)};
                    line.b = {static_cast<int64_t>(a.x), static_cast<int64_t>(a.y)};
                    line.aId = bIdx; line.bId = aIdx;
                } else {
                    line.a = {static_cast<int64_t>(a.x), static_cast<int64_t>(a.y)};
                    line.b = {static_cast<int64_t>(b.x), static_cast<int64_t>(b.y)};
                    line.aId = aIdx; line.bId = bIdx;
                }
            } else if (facet.vertex[0].z < sliceZ || facet.vertex[1].z < sliceZ || facet.vertex[2].z < sliceZ) {
                line.edgeType = IntersectionLine::Top;
                line.a = {static_cast<int64_t>(b.x), static_cast<int64_t>(b.y)};
                line.b = {static_cast<int64_t>(a.x), static_cast<int64_t>(a.y)};
                line.aId = bIdx; line.bId = aIdx;
            } else {
                line.edgeType = IntersectionLine::Bottom;
                line.a = {static_cast<int64_t>(a.x), static_cast<int64_t>(a.y)};
                line.b = {static_cast<int64_t>(b.x), static_cast<int64_t>(b.y)};
                line.aId = aIdx; line.bId = bIdx;
            }
            lines.push_back(line);
            foundHorizontalEdge = true;
            if (line.edgeType != IntersectionLine::Horizontal) return;
        } else if (aZ == sliceZ) {
            IntersectionPoint pt;
            pt.x = static_cast<int64_t>(a.x);
            pt.y = static_cast<int64_t>(a.y);
            pt.pointId = aIdx;
            points.push_back(pt);
            pointsOnLayer.push_back(points.size() - 1);
        } else if (bZ == sliceZ) {
            IntersectionPoint pt;
            pt.x = static_cast<int64_t>(b.x);
            pt.y = static_cast<int64_t>(b.y);
            pt.pointId = bIdx;
            points.push_back(pt);
            pointsOnLayer.push_back(points.size() - 1);
        } else if ((aZ < sliceZ && bZ > sliceZ) || (bZ < sliceZ && aZ > sliceZ)) {
            // Edge crosses the slicing plane
            float t = (scaledZ - b.z) / (a.z - b.z);
            IntersectionPoint pt;
            pt.x = static_cast<int64_t>(b.x + (a.x - b.x) * t);
            pt.y = static_cast<int64_t>(b.y + (a.y - b.y) * t);
            pt.edgeId = edgeId;
            points.push_back(pt);
        }
    }

    if (foundHorizontalEdge) return;

    // Handle duplicate on-layer points
    if (!pointsOnLayer.empty()) {
        if (pointsOnLayer.size() == 2 && points.size() >= 3) {
            points.erase(points.begin() + pointsOnLayer[1]);
        } else if (points.size() < 3) {
            return;  // V-shaped tangent, no real intersection
        }
    }

    if (points.size() == 2) {
        IntersectionLine line;
        line.a = {points[1].x, points[1].y};
        line.b = {points[0].x, points[0].y};
        line.aId = points[1].pointId;
        line.bId = points[0].pointId;
        line.edgeAId = points[1].edgeId;
        line.edgeBId = points[0].edgeId;
        lines.push_back(line);
    }
}

// ==============================================================================
// Slicing: Loop Chaining (ported from Legacy make_loops)
// ==============================================================================

void TriMesh::makeLoops(IntersectionLines& lines, Polygons2i& loops) const {
    // -------------------------------------------------------------------------
    // Remove duplicate tangent edges.
    //
    // Two lines are duplicates iff they share VALID (non-negative) vertex/edge
    // IDs for BOTH endpoints.  Comparing -1 == -1 is meaningless — two
    // cross-edge intersection points with aId==-1 are on completely different
    // edges and must NOT be deduplicated.
    //
    // Rules (ported from Legacy TriangleMeshSlicer::_make_loops):
    //   - If two lines have the same valid (aId,bId) pair → mark the second
    //     skip=true.  If they also share the same edgeType → mark both skip.
    //   - Horizontal edges (both aId>=0 and bId>=0 and edgeType=Horizontal)
    //     that are mirror-reversed of each other are both skipped.
    // -------------------------------------------------------------------------
    for (size_t i = 0; i < lines.size(); ++i) {
        auto& line = lines[i];
        if (line.skip || line.edgeType == IntersectionLine::None) continue;

        for (size_t j = i + 1; j < lines.size(); ++j) {
            auto& line2 = lines[j];
            if (line2.skip || line2.edgeType == IntersectionLine::None) continue;

            // Only deduplicate when BOTH aId and bId are valid (non-negative).
            // Cross-edge intersection points have aId==-1 and must not match.
            bool aIdsValid = (line.aId >= 0) && (line2.aId >= 0);
            bool bIdsValid = (line.bId >= 0) && (line2.bId >= 0);

            if (aIdsValid && bIdsValid) {
                if (line.aId == line2.aId && line.bId == line2.bId) {
                    line2.skip = true;
                    if (line.edgeType == line2.edgeType) {
                        line.skip = true;
                        break;
                    }
                } else if (line.aId == line2.bId && line.bId == line2.aId) {
                    // Mirror pair (horizontal edges meeting from opposite sides)
                    if (line.edgeType == IntersectionLine::Horizontal &&
                        line2.edgeType == IntersectionLine::Horizontal) {
                        line.skip  = true;
                        line2.skip = true;
                        break;
                    }
                }
            }
        }
    }

    // -------------------------------------------------------------------------
    // Build lookup maps for chaining.
    // -------------------------------------------------------------------------
    int maxEdgeId = 0;
    int maxVertId = static_cast<int>(sharedVertices_.size());
    for (const auto& line : lines) {
        if (line.edgeAId > maxEdgeId) maxEdgeId = line.edgeAId;
        if (line.edgeBId > maxEdgeId) maxEdgeId = line.edgeBId;
    }

    std::vector<std::vector<IntersectionLine*>> byEdgeA(maxEdgeId + 2);
    std::vector<std::vector<IntersectionLine*>> byA(maxVertId + 2);

    for (auto& line : lines) {
        if (line.skip) continue;
        if (line.edgeAId >= 0 && line.edgeAId <= maxEdgeId)
            byEdgeA[line.edgeAId].push_back(&line);
        if (line.aId >= 0 && line.aId < maxVertId)
            byA[line.aId].push_back(&line);
    }

    // -------------------------------------------------------------------------
    // Chain lines into closed polygon loops.
    // -------------------------------------------------------------------------
    while (true) {
        // Find first unused line
        IntersectionLine* first = nullptr;
        for (auto& line : lines) {
            if (!line.skip) { first = &line; break; }
        }
        if (!first) break;

        first->skip = true;
        std::vector<IntersectionLine*> loop;
        loop.push_back(first);

        while (true) {
            IntersectionLine* current = loop.back();
            IntersectionLine* next = nullptr;

            // Try to find next line by edgeBId match
            if (current->edgeBId >= 0 && current->edgeBId <= maxEdgeId) {
                for (auto* candidate : byEdgeA[current->edgeBId]) {
                    if (!candidate->skip) { next = candidate; break; }
                }
            }
            // Try by vertex bId match
            if (!next && current->bId >= 0 && current->bId < maxVertId) {
                for (auto* candidate : byA[current->bId]) {
                    if (!candidate->skip) { next = candidate; break; }
                }
            }

            if (!next) {
                // Check if loop is closed
                IntersectionLine* firstLine = loop.front();
                bool closed = false;
                if (firstLine->edgeAId >= 0 && firstLine->edgeAId == current->edgeBId) closed = true;
                if (!closed && firstLine->aId >= 0 && firstLine->aId == current->bId) closed = true;
                // Cross-edge loops close when the chain returns to the first line's starting edge
                if (!closed && firstLine->edgeAId >= 0 && current->edgeBId >= 0 &&
                    firstLine->edgeAId == current->edgeBId) closed = true;
                // Allow closing via edge matching even when IDs are -1 (cross-edge)
                // by checking if the loop has enough points to form a valid polygon
                if (!closed && loop.size() >= 3) {
                    // If we can't find a next segment, the loop may still be
                    // geometrically closed. Accept it if it has ≥3 segments.
                    closed = true;
                }

                if (closed && loop.size() >= 3) {
                    Polygon2i poly;
                    poly.reserve(loop.size());
                    for (const auto* l : loop) {
                        poly.push_back(l->a);
                    }
                    loops.push_back(std::move(poly));
                }
                break;
            }

            loop.push_back(next);
            next->skip = true;
        }
    }
}

// ==============================================================================
// Slicing: ExPolygon Construction (ported from Legacy make_expolygons)
// ==============================================================================

void TriMesh::makeExPolygons(const Polygons2i& loops, ExPolygons2i& slices) const {
    if (loops.empty()) return;

    // -------------------------------------------------------------------------
    // Step 1: Convert all loops to Clipper2 paths preserving signed area.
    //
    // We do NOT attempt to classify loops by winding before feeding them to
    // Clipper2.  Instead we feed the raw loops (as output by makeLoops) into a
    // single Clipper2 Union with FillRule::NonZero.  NonZero handles any mix of
    // CW/CCW winding and self-intersections that result from the triangle–plane
    // intersection stage.  The output PolyTree64 then unambiguously tells us,
    // for each output polygon, whether it is an outer contour (IsHole()==false)
    // or a hole (IsHole()==true).
    // -------------------------------------------------------------------------
    Clipper2Lib::Paths64 inputPaths;
    inputPaths.reserve(loops.size());
    for (const auto& loop : loops) {
        if (loop.size() < 3) continue;
        Clipper2Lib::Path64 path;
        path.reserve(loop.size());
        for (const auto& pt : loop) {
            path.emplace_back(pt.x, pt.y);
        }
        inputPaths.push_back(std::move(path));
    }

    if (inputPaths.empty()) return;

    // -------------------------------------------------------------------------
    // Step 2: Safety offset — closes tiny gaps left by numerical imprecision
    //         during the facet-plane intersection stage.  Equivalent to the
    //         Legacy offset2_ex( +eps, -eps ) call.
    //
    //  MESH_SCALING_FACTOR = 1e-6  →  1 Clipper2 unit = 1 nm
    //  safetyOffset of 0.05 mm  →  0.05 / 1e-6 = 50 000 Clipper2 units
    // -------------------------------------------------------------------------
    const double safetyOffset = 0.05 / MESH_SCALING_FACTOR;  // 50 000 nm
    Clipper2Lib::Paths64 grown = Clipper2Lib::InflatePaths(
        inputPaths, safetyOffset,
        Clipper2Lib::JoinType::Miter,
        Clipper2Lib::EndType::Polygon);
    Clipper2Lib::Paths64 shrunk = Clipper2Lib::InflatePaths(
        grown, -safetyOffset,
        Clipper2Lib::JoinType::Miter,
        Clipper2Lib::EndType::Polygon);

    if (shrunk.empty()) {
        // Fallback: use the raw paths if the inflate/deflate collapsed everything
        shrunk = std::move(inputPaths);
    }

    // -------------------------------------------------------------------------
    // Step 3: Union into a PolyTree64 with NonZero fill rule.
    //
    //  NonZero correctly handles:
    //    - CCW outer contours    (winding number = +1 → solid)
    //    - CW inner holes        (winding number =  0 → void)
    //    - Nested islands        (winding number = +1 → solid)
    //    - Any raw slicer output regardless of initial winding direction
    //
    //  The resulting PolyTree64 hierarchy is unambiguous:
    //    Root → [level-1 outer contours]
    //             └─ [level-2 holes]
    //                  └─ [level-3 nested islands]
    //                       └─ ...
    // -------------------------------------------------------------------------
    Clipper2Lib::Clipper64 unionClipper;
    unionClipper.AddSubject(shrunk);
    Clipper2Lib::PolyTree64 polyTree;
    Clipper2Lib::Paths64 openPaths;
    unionClipper.Execute(Clipper2Lib::ClipType::Union,
                         Clipper2Lib::FillRule::NonZero,
                         polyTree, openPaths);

    // -------------------------------------------------------------------------
    // Step 4: Walk the PolyTree and build ExPolygon2i objects.
    //
    //  Per Clipper2 docs and the IsHole() implementation above:
    //    Level 1 nodes  →  outer contours   (IsHole() == false)
    //    Level 2 nodes  →  holes            (IsHole() == true)
    //    Level 3 nodes  →  nested islands   (IsHole() == false)
    //    Level 4 nodes  →  holes inside islands  ...and so on.
    //
    //  We build one ExPolygon2i per outer-contour node.  Its direct hole
    //  children are attached as holes.  Nested islands inside holes are each
    //  recursed into as independent outer contours (they become separate
    //  ExPolygon2i entries).
    // -------------------------------------------------------------------------
    std::function<void(const Clipper2Lib::PolyPath64&)> processNode;
    processNode = [&slices, &processNode](const Clipper2Lib::PolyPath64& node) {

        if (node.IsHole()) {
            // Should not be called directly on a hole — recurse into its
            // children (which are nested solid islands) instead.
            for (const auto& child : node) {
                processNode(*child);
            }
            return;
        }

        if (node.Polygon().empty()) {
            // Root node or empty polygon — just recurse into children.
            for (const auto& child : node) {
                processNode(*child);
            }
            return;
        }

        // This node is an outer contour.
        ExPolygon2i exPoly;
        for (const auto& pt : node.Polygon()) {
            exPoly.contour.push_back({pt.x, pt.y});
        }

        // Direct children of this contour are holes.
        for (const auto& holeChild : node) {
            if (holeChild->Polygon().empty()) continue;

            Polygon2i holeLoop;
            for (const auto& pt : holeChild->Polygon()) {
                holeLoop.push_back({pt.x, pt.y});
            }
            exPoly.holes.push_back(std::move(holeLoop));

            // Grandchildren of the hole are nested solid islands —
            // recursed into as completely independent ExPolygons.
            for (const auto& island : *holeChild) {
                processNode(*island);
            }
        }

        slices.push_back(std::move(exPoly));
    };

    // Process all top-level (level-1) nodes of the tree.
    for (const auto& topLevel : polyTree) {
        processNode(*topLevel);
    }
}

} // namespace Geometry
} // namespace MarcSLM
