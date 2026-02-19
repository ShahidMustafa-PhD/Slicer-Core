// ==============================================================================
// MarcSLM - TriMesh: Industrial Triangle Mesh with Repair
// ==============================================================================
// Ported and refactored from Legacy Slic3r::TriangleMesh + admesh concepts.
// Provides Assimp-based loading, topology repair, and direct Z-plane slicing.
// ==============================================================================

#pragma once

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace MarcSLM {
namespace Geometry {

// ==============================================================================
// Fundamental Types (scaled integer coordinates, matching Legacy approach)
// ==============================================================================

/// Scaling factor: 1 unit = 1 nanometre when input is in mm.
/// This is the same value used by Slic3r/PrusaSlicer for robust integer math.
constexpr double MESH_SCALING_FACTOR = 0.000001;

inline constexpr int64_t meshScale(double val)   { return static_cast<int64_t>(val / MESH_SCALING_FACTOR); }
inline constexpr double  meshUnscale(int64_t val) { return val * MESH_SCALING_FACTOR; }

// ==============================================================================
// Vertex / Facet structures (float, matching STL files)
// ==============================================================================

struct Vertex3f {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    Vertex3f() = default;
    Vertex3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};

struct Facet {
    Vertex3f normal;
    Vertex3f vertex[3];
};

struct FacetNeighbors {
    int neighbor[3] = {-1, -1, -1};   ///< Adjacent facet index per edge (-1 = boundary)
};

struct VertexIndices {
    int vertex[3] = {-1, -1, -1};     ///< Shared vertex index per corner
};

// ==============================================================================
// Bounding Box
// ==============================================================================

struct BBox3f {
    Vertex3f min{std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max(),
                 std::numeric_limits<float>::max()};
    Vertex3f max{std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest(),
                 std::numeric_limits<float>::lowest()};

    void merge(const Vertex3f& v) {
        if (v.x < min.x) min.x = v.x;
        if (v.y < min.y) min.y = v.y;
        if (v.z < min.z) min.z = v.z;
        if (v.x > max.x) max.x = v.x;
        if (v.y > max.y) max.y = v.y;
        if (v.z > max.z) max.z = v.z;
    }

    [[nodiscard]] float sizeX() const { return max.x - min.x; }
    [[nodiscard]] float sizeY() const { return max.y - min.y; }
    [[nodiscard]] float sizeZ() const { return max.z - min.z; }
    [[nodiscard]] bool  valid() const { return min.x <= max.x; }
};

// ==============================================================================
// MeshStats: repair / topology statistics
// ==============================================================================

struct MeshStats {
    size_t numFacets          = 0;
    size_t numSharedVertices  = 0;
    size_t connectedFacets1Edge = 0;   ///< Facets with ?1 connected edge
    size_t connectedFacets2Edge = 0;   ///< Facets with ?2 connected edges
    size_t connectedFacets3Edge = 0;   ///< Facets with all 3 edges connected
    size_t degenerateFacets   = 0;
    size_t edgesFixed         = 0;
    size_t facetsRemoved      = 0;
    size_t facetsAdded        = 0;
    size_t facetsReversed     = 0;
    size_t normalsFixed       = 0;
    float  volume             = 0.0f;
    float  shortestEdge       = 0.0f;
    float  boundingDiameter   = 0.0f;
};

// ==============================================================================
// 2D Polygon types for slicing output (scaled integer coordinates)
// ==============================================================================

struct Point2i {
    int64_t x = 0, y = 0;
    Point2i() = default;
    Point2i(int64_t x_, int64_t y_) : x(x_), y(y_) {}
};

using Polygon2i  = std::vector<Point2i>;
using Polygons2i = std::vector<Polygon2i>;

/// ExPolygon: a contour with zero or more holes (ported from Legacy)
struct ExPolygon2i {
    Polygon2i contour;
    Polygons2i holes;
};
using ExPolygons2i = std::vector<ExPolygon2i>;

// ==============================================================================
// TriMesh: The Core Triangle Mesh Class
// ==============================================================================

/// @brief Industrial-grade triangle mesh with topology repair and Z-plane slicing.
/// @details Ported from Legacy Slic3r code using admesh-inspired algorithms.
///          Accepts meshes loaded via Assimp and provides:
///          - Automatic topology repair (edge merging, hole filling, normal fixing)
///          - Robust Z-plane slicing via triangle-plane intersection
///          - ExPolygon output with proper contour/hole classification
class TriMesh {
public:
    TriMesh();
    ~TriMesh();

    TriMesh(const TriMesh& other);
    TriMesh& operator=(const TriMesh& other);
    TriMesh(TriMesh&& other) noexcept;
    TriMesh& operator=(TriMesh&& other) noexcept;

    // =========================================================================
    // Construction from raw data (Assimp output)
    // =========================================================================

    /// @brief Build mesh from vertex array and face index array.
    /// @param vertices  Pointer to vertex positions (Nx3 floats).
    /// @param numVerts  Number of vertices.
    /// @param indices   Pointer to triangle indices (Mx3 uint32_t).
    /// @param numFaces  Number of triangles.
    void buildFromArrays(const float* vertices, size_t numVerts,
                         const uint32_t* indices, size_t numFaces);

    // =========================================================================
    // Repair
    // =========================================================================

    /// @brief Perform full topology repair (edge matching, hole filling, normal fixing).
    /// @details This is the core repair algorithm ported from admesh/Slic3r.
    ///          It makes non-manifold STL files sliceable.
    void repair();

    /// @brief Check if the mesh has been repaired.
    [[nodiscard]] bool isRepaired() const { return repaired_; }

    /// @brief Check if mesh is manifold (all edges shared by exactly 2 faces).
    [[nodiscard]] bool isManifold() const;

    /// @brief Did the mesh require any repair?
    [[nodiscard]] bool neededRepair() const;

    // =========================================================================
    // Queries
    // =========================================================================

    [[nodiscard]] size_t facetCount()  const { return facets_.size(); }
    [[nodiscard]] size_t vertexCount() const { return sharedVertices_.size(); }
    [[nodiscard]] bool   empty()       const { return facets_.empty(); }
    [[nodiscard]] const BBox3f&    bbox()  const { return bbox_; }
    [[nodiscard]] const MeshStats& stats() const { return stats_; }
    [[nodiscard]] float  volume();

    // =========================================================================
    // Slicing
    // =========================================================================

    /// @brief Slice the mesh at multiple Z-heights simultaneously.
    /// @param zHeights  Sorted vector of Z-coordinates (unscaled, in mm).
    /// @return Vector of ExPolygon sets, one per Z-height.
    ///
    /// Algorithm (ported from Legacy TriangleMeshSlicer<Z>):
    ///   1. Build edge-facet adjacency tables
    ///   2. For each facet, find which Z-planes it intersects
    ///   3. Compute intersection line segments per Z-plane
    ///   4. Chain line segments into closed polygon loops
    ///   5. Classify loops into contours vs holes using winding/area
    ///   6. Build ExPolygon (contour + holes) using Clipper2 boolean ops
    [[nodiscard]] std::vector<ExPolygons2i> slice(const std::vector<float>& zHeights);

private:
    // =========================================================================
    // Internal Data
    // =========================================================================

    std::vector<Facet>            facets_;           ///< Triangle facet list
    std::vector<FacetNeighbors>   neighbors_;        ///< Per-facet neighbor info
    std::vector<VertexIndices>    vertexIndices_;     ///< Per-facet shared vertex indices
    std::vector<Vertex3f>         sharedVertices_;   ///< Shared (welded) vertex array
    BBox3f                        bbox_;
    MeshStats                     stats_;
    bool                          repaired_ = false;

    // =========================================================================
    // Repair Internals
    // =========================================================================

    void computeBBox();
    void computeStats();
    void checkTopologyExact();
    void checkTopologyNearby(float tolerance);
    void fixNormals();
    void fillHoles();
    void removeDegenerate();
    void generateSharedVertices();
    void calculateVolume();

    // =========================================================================
    // Slicer Internals (ported from TriangleMeshSlicer<Z>)
    // =========================================================================

    /// Intersection point on a Z-plane
    struct IntersectionPoint {
        int64_t x = 0, y = 0;
        int pointId = -1;   ///< Shared vertex index (if on vertex)
        int edgeId  = -1;   ///< Edge index (if between vertices)
    };

    /// Intersection line segment on a Z-plane
    struct IntersectionLine {
        Point2i a, b;
        int aId      = -1;   ///< Shared vertex index of point a
        int bId      = -1;   ///< Shared vertex index of point b
        int edgeAId  = -1;   ///< Edge index for point a
        int edgeBId  = -1;   ///< Edge index for point b
        enum EdgeType { None, Top, Bottom, Horizontal } edgeType = None;
        bool skip = false;
    };

    using IntersectionLines = std::vector<IntersectionLine>;

    /// @brief Build edge-to-facet mapping tables required by the slicer.
    /// @return Per-facet edge index table (facet_idx ? [edge0, edge1, edge2]).
    std::vector<std::vector<int>> buildFacetsEdges() const;

    /// @brief Slice a single facet against a Z-plane.
    void sliceFacet(float scaledZ, const Facet& facet, int facetIdx,
                    float minZ, float maxZ,
                    const std::vector<std::vector<int>>& facetsEdges,
                    const Vertex3f* scaledShared,
                    IntersectionLines& lines) const;

    /// @brief Chain intersection line segments into closed polygon loops.
    void makeLoops(IntersectionLines& lines, Polygons2i& loops) const;

    /// @brief Convert polygon loops into ExPolygons (contour + holes).
    void makeExPolygons(const Polygons2i& loops, ExPolygons2i& slices) const;
};

} // namespace Geometry
} // namespace MarcSLM
