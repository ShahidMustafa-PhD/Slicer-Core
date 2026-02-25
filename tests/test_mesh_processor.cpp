// ==============================================================================
// MarcSLM - Mesh Processor Unit Tests
// ==============================================================================
// Tests for MeshProcessor slicing engine (TriMesh-based, no Manifold)
// ==============================================================================

#include <MarcSLM/Geometry/MeshProcessor.hpp>
#include <MarcSLM/Geometry/TriMesh.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

using namespace MarcSLM::Geometry;

namespace {

// ==============================================================================
// Helper: create a cube TriMesh directly
// ==============================================================================

/// Build a 2󫎾 cube centered at origin: vertices from (-1,-1,-1) to (1,1,1).
static std::unique_ptr<TriMesh> makeCubeTriMesh() {
    const float verts[] = {
        // 0: bottom-left-front   1: bottom-right-front
        -1.f, -1.f, -1.f,         1.f, -1.f, -1.f,
        // 2: bottom-right-back   3: bottom-left-back
         1.f,  1.f, -1.f,        -1.f,  1.f, -1.f,
        // 4: top-left-front      5: top-right-front
        -1.f, -1.f,  1.f,         1.f, -1.f,  1.f,
        // 6: top-right-back      7: top-left-back
         1.f,  1.f,  1.f,        -1.f,  1.f,  1.f
    };

    const uint32_t tris[] = {
        // Bottom face
        0, 2, 1,   0, 3, 2,
        // Top face
        4, 5, 6,   4, 6, 7,
        // Front face
        0, 1, 5,   0, 5, 4,
        // Back face
        3, 6, 2,   3, 7, 6,
        // Left face
        0, 4, 7,   0, 7, 3,
        // Right face
        1, 2, 6,   1, 6, 5
    };

    auto mesh = std::make_unique<TriMesh>();
    mesh->buildFromArrays(verts, 8, tris, 12);
    mesh->repair();
    return mesh;
}

// ==============================================================================
// Test Fixtures
// ==============================================================================

class MeshProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_unique<MeshProcessor>();
    }
    std::unique_ptr<MeshProcessor> processor_;
};

class MeshProcessorWithCubeTest : public MeshProcessorTest {
protected:
    void SetUp() override {
        MeshProcessorTest::SetUp();
        processor_->mesh_ = makeCubeTriMesh();
        processor_->bbox_ = processor_->mesh_->bbox();
    }
};

} // anonymous namespace

// ==============================================================================
// Basic Construction and State
// ==============================================================================

TEST_F(MeshProcessorTest, DefaultConstruction) {
    EXPECT_FALSE(processor_->hasValidMesh());
}

TEST_F(MeshProcessorTest, MoveOperations) {
    auto processor2 = std::move(*processor_);
    EXPECT_FALSE(processor_->hasValidMesh());
}

// ==============================================================================
// Mesh Loading Tests
// ==============================================================================

TEST_F(MeshProcessorTest, LoadMesh_InvalidFile) {
    EXPECT_THROW(processor_->loadMesh("nonexistent.stl"), MeshLoadError);
}

TEST_F(MeshProcessorTest, LoadMesh_EmptyPath) {
    EXPECT_THROW(processor_->loadMesh(""), MeshLoadError);
}

// ==============================================================================
// Cube Mesh Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, HasValidMesh) {
    EXPECT_TRUE(processor_->hasValidMesh());
}

TEST_F(MeshProcessorWithCubeTest, GetBoundingBox) {
    BBox3f bb = processor_->getBoundingBox();
    EXPECT_FLOAT_EQ(bb.min.x, -1.0f);
    EXPECT_FLOAT_EQ(bb.min.y, -1.0f);
    EXPECT_FLOAT_EQ(bb.min.z, -1.0f);
    EXPECT_FLOAT_EQ(bb.max.x,  1.0f);
    EXPECT_FLOAT_EQ(bb.max.y,  1.0f);
    EXPECT_FLOAT_EQ(bb.max.z,  1.0f);
}

TEST_F(MeshProcessorWithCubeTest, GetMeshStats) {
    auto [verts, tris, volume] = processor_->getMeshStats();
    EXPECT_EQ(tris, 12u);
    EXPECT_GT(volume, 0.0);
}

// ==============================================================================
// Slicing Parameter Validation
// ==============================================================================

TEST_F(MeshProcessorTest, SliceUniform_InvalidThickness) {
    EXPECT_THROW(processor_->sliceUniform(0.0f), SlicingError);
    EXPECT_THROW(processor_->sliceUniform(-1.0f), SlicingError);
    EXPECT_THROW(processor_->sliceUniform(0.0001f), SlicingError);
    EXPECT_THROW(processor_->sliceUniform(100.0f), SlicingError);
}

TEST_F(MeshProcessorTest, SliceAdaptive_InvalidParams) {
    EXPECT_THROW(processor_->sliceAdaptive(0.1f, 0.05f, 0.1f), SlicingError);
    EXPECT_THROW(processor_->sliceAdaptive(-0.1f, 0.1f, 0.1f), SlicingError);
    EXPECT_THROW(processor_->sliceAdaptive(0.1f, 0.2f, -0.1f), SlicingError);
    EXPECT_THROW(processor_->sliceAdaptive(0.1f, 0.2f, 1.5f), SlicingError);
}

TEST_F(MeshProcessorTest, SliceUniform_NoMesh) {
    EXPECT_THROW(processor_->sliceUniform(0.1f), MeshProcessingError);
}

TEST_F(MeshProcessorTest, SliceAdaptive_NoMesh) {
    EXPECT_THROW(processor_->sliceAdaptive(0.01f, 0.1f), MeshProcessingError);
}

TEST_F(MeshProcessorTest, SliceAtHeight_NoMesh) {
    EXPECT_THROW(processor_->sliceAtHeight(0.0f), MeshProcessingError);
}

// ==============================================================================
// Uniform Slicing Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, SliceUniform_Cube) {
    LayerStack layers = processor_->sliceUniform(0.5f);
    // Cube height 2.0 / 0.5 = 4 layers
    ASSERT_EQ(layers.size(), 4u);

    for (size_t i = 0; i < layers.size(); ++i) {
        EXPECT_EQ(layers[i].layerNumber, i);
        EXPECT_FLOAT_EQ(layers[i].layerThickness, 0.5f);
    }
}

TEST_F(MeshProcessorWithCubeTest, SliceUniform_ProducesPolylines) {
    LayerStack layers = processor_->sliceUniform(0.5f);
    // At least some layers should have polylines (contour of the cube)
    bool anyPolylines = false;
    for (const auto& layer : layers) {
        if (!layer.polylines.empty()) {
            anyPolylines = true;
            // Cube cross-section is a rectangle ? ?4 vertices in closed polyline
            EXPECT_GE(layer.polylines[0].points.size(), 4u);
        }
    }
    EXPECT_TRUE(anyPolylines);
}

TEST_F(MeshProcessorWithCubeTest, SliceUniform_ThickLayers) {
    LayerStack layers = processor_->sliceUniform(2.5f);
    EXPECT_GE(layers.size(), 1u);
}

// ==============================================================================
// Adaptive Slicing Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, SliceAdaptive_Cube) {
    LayerStack layers = processor_->sliceAdaptive(0.01f, 0.1f, 0.05f);
    EXPECT_GE(layers.size(), 1u);

    for (const auto& layer : layers) {
        EXPECT_GE(layer.layerThickness, 0.01f - 1e-6f);
        EXPECT_LE(layer.layerThickness, 0.1f + 1e-6f);
    }
}

// ==============================================================================
// Single Height Slicing Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, SliceAtHeight_CubeCenter) {
    LayerSlices layer = processor_->sliceAtHeight(0.0f);
    EXPECT_EQ(layer.layerNumber, 0u);
    EXPECT_FALSE(layer.polylines.empty());
}

TEST_F(MeshProcessorWithCubeTest, SliceAtHeight_OutsideBounds) {
    LayerSlices layer = processor_->sliceAtHeight(5.0f);
    EXPECT_TRUE(layer.polylines.empty());
}

TEST_F(MeshProcessorWithCubeTest, SliceAtHeights_Multiple) {
    std::vector<float> heights = {-0.5f, 0.0f, 0.5f};
    LayerStack layers = processor_->sliceAtHeights(heights);
    ASSERT_EQ(layers.size(), 3u);
    for (size_t i = 0; i < layers.size(); ++i) {
        EXPECT_EQ(layers[i].layerNumber, i);
    }
}

TEST_F(MeshProcessorWithCubeTest, SliceAtHeights_Empty) {
    LayerStack layers = processor_->sliceAtHeights({});
    EXPECT_TRUE(layers.empty());
}

// ==============================================================================
// Exception Tests
// ==============================================================================

TEST_F(MeshProcessorTest, GetBoundingBox_NoMesh) {
    EXPECT_THROW(processor_->getBoundingBox(), MeshProcessingError);
}

TEST_F(MeshProcessorTest, GetMeshStats_NoMesh) {
    EXPECT_THROW(processor_->getMeshStats(), MeshProcessingError);
}

// ==============================================================================
// Performance Sanity Check
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, Performance_UniformSlicing) {
    auto start = std::chrono::high_resolution_clock::now();
    LayerStack layers = processor_->sliceUniform(0.1f);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 5000);
    EXPECT_GE(layers.size(), 10u);
}

// ==============================================================================
// Hole-Aware Slicing + Hatching Integration Tests
// ==============================================================================

#include <MarcSLM/PathPlanning/HatchGenerator.hpp>
#include <MarcSLM/Core/SlmConfig.hpp>
#include <MarcSLM/Core/Types.hpp>

namespace {

/// @brief Build a hollow square tube mesh.
///
/// The tube has:
///   - Outer square cross-section from (-R,-R) to (+R,+R)
///   - Inner square hole from (-r,-r) to (+r,+r)
///   - Height from z=0 to z=H
///
/// This creates a mesh whose mid-layer slice should be an ExPolygon with
/// one contour (outer square) and one hole (inner square).
static std::unique_ptr<TriMesh> makeHollowSquareTube(
    float outerR, float innerR, float H) {
    // Vertices:
    //   0-3:  outer bottom square (CCW from +Z = x-y plane)
    //   4-7:  outer top square
    //   8-11: inner bottom square (CW from +Z, so normals point inward = toward hole centre)
    //   12-15: inner top square

    const float o = outerR;
    const float r = innerR;

    const float verts[] = {
        // Outer bottom (0-3) � CCW from above (+Z) so normal = +Z for bottom face
        -o, -o, 0,   o, -o, 0,   o,  o, 0,  -o,  o, 0,
        // Outer top (4-7)
        -o, -o, H,   o, -o, H,   o,  o, H,  -o,  o, H,
        // Inner bottom (8-11) � CW from above (+Z) so normal = -Z for inner bottom face
        -r, -r, 0,   r, -r, 0,   r,  r, 0,  -r,  r, 0,
        // Inner top (12-15)
        -r, -r, H,   r, -r, H,   r,  r, H,  -r,  r, H,
    };

    // Face winding conventions:
    //   Outer faces (top/bottom/walls): CCW when viewed from OUTSIDE the solid
    //   Inner faces (top/bottom/walls): CCW when viewed from INSIDE the hole
    //   For a manifold hollow solid, inner faces must have normals pointing
    //   INTO the solid (toward the hole centre), meaning CW when viewed from outside.
    const uint32_t tris[] = {
        // Outer bottom face (CCW from below = CW from +Z, normal points -Z)
        0, 1, 2,   0, 2, 3,
        // Outer top face (CCW from above = +Z normal)
        4, 6, 5,   4, 7, 6,
        // Outer walls (normals point outward):
        0, 5, 1,   0, 4, 5,   // front (y=-o): normal = -Y
        1, 6, 2,   1, 5, 6,   // right (x=+o): normal = +X
        2, 7, 3,   2, 6, 7,   // back  (y=+o): normal = +Y
        3, 4, 0,   3, 7, 4,   // left  (x=-o): normal = -X
        // Inner bottom face (CCW from above = +Z normal; acts as lid of the hole)
        8, 10, 9,   8, 11, 10,
        // Inner top face
        12, 13, 14,  12, 14, 15,
        // Inner walls (normals point INWARD, toward hole centre):
        8,  9, 13,   8, 13, 12,  // front inner (y=-r): normal = +Y (inward)
        9, 10, 14,   9, 14, 13,  // right inner (x=+r): normal = -X (inward)
        10, 11, 15,  10, 15, 14, // back  inner (y=+r): normal = -Y (inward)
        11,  8, 12,  11, 12, 15, // left  inner (x=-r): normal = +X (inward)
    };

    const size_t numVerts = sizeof(verts) / (3 * sizeof(float));
    const size_t numTris  = sizeof(tris)  / (3 * sizeof(uint32_t));

    auto mesh = std::make_unique<TriMesh>();
    mesh->buildFromArrays(verts, numVerts, tris, numTris);
    mesh->repair();
    return mesh;
}

} // anonymous namespace

// ==============================================================================
// Hollow Tube: Slicing produces ExPolygons with holes
// ==============================================================================

class HollowTubeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 20�20mm outer, 10�10mm inner hole, 2mm tall
        auto mesh = makeHollowSquareTube(10.0f, 5.0f, 2.0f);
        processor_.mesh_ = std::move(mesh);
        processor_.bbox_ = processor_.mesh_->bbox();
    }
    MeshProcessor processor_;
};

TEST_F(HollowTubeTest, SliceProducesExPolygonsWithHoles) {
    // Slice at mid-height (z = 1.0 mm)
    auto layers = processor_.sliceAtHeights({1.0f});
    ASSERT_EQ(layers.size(), 1u);

    const auto& layer = layers[0];

    // Must have ExPolygons
    ASSERT_FALSE(layer.exPolygons.empty())
        << "Hollow tube slice must produce at least one ExPolygon";

    // The ExPolygon must have at least one hole
    bool foundHole = false;
    for (const auto& exPoly : layer.exPolygons) {
        if (exPoly.hasHoles()) {
            foundHole = true;
            break;
        }
    }
    EXPECT_TRUE(foundHole)
        << "Hollow tube cross-section must have at least one hole (inner void)";
}

TEST_F(HollowTubeTest, HatchesDontPassThroughHole) {
    MarcSLM::SlmConfig config;
    config.hatch_spacing = 0.3;   // 300 祄 spacing for a fast test
    config.hatch_angle   = 0.0;   // horizontal hatches

    auto layers = processor_.sliceAtHeights({1.0f});
    ASSERT_EQ(layers.size(), 1u);

    auto& layer = layers[0];
    MarcSLM::PathPlanning::HatchGenerator gen(config);

    // Generate hatches using ExPolygons (contour + holes)
    for (const auto& exPoly : layer.exPolygons) {
        if (!exPoly.isValid()) continue;

        // Convert contour to Clipper2
        Clipper2Lib::Path64 contourPath;
        for (const auto& pt : exPoly.contour.points) {
            contourPath.emplace_back(
                MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.x)),
                MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.y)));
        }

        // Convert holes to Clipper2
        Clipper2Lib::Paths64 holePaths;
        for (const auto& hole : exPoly.holes) {
            Clipper2Lib::Path64 holePath;
            for (const auto& pt : hole.points) {
                holePath.emplace_back(
                    MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.x)),
                    MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.y)));
            }
            if (holePath.size() >= 3) holePaths.push_back(std::move(holePath));
        }

        if (!exPoly.hasHoles()) continue;  // Only test cross-sections with holes

        auto hatchLines = gen.generateHatches(contourPath, holePaths, 0.0);
        ASSERT_FALSE(hatchLines.empty())
            << "Hollow tube annular region must produce hatches";

        // Verify: no hatch line midpoint falls inside the inner square hole.
        // The inner hole is the region -5 < x < 5 AND -5 < y < 5.
        // (inner square is 10�10mm centered at origin = [-5,5] � [-5,5])
        // Use a small inset of 0.5mm to account for endpoint overlap / boundary effects.
        const float holeMargin = 0.5f;
        const float holeMin = -5.0f + holeMargin;
        const float holeMax =  5.0f - holeMargin;

        int insideHole = 0;
        for (const auto& line : hatchLines) {
            float mx = (line.a.x + line.b.x) * 0.5f;
            float my = (line.a.y + line.b.y) * 0.5f;
            if (mx > holeMin && mx < holeMax &&
                my > holeMin && my < holeMax) {
                ++insideHole;
            }
        }

        EXPECT_EQ(insideHole, 0)
            << "Found " << insideHole
            << " hatch line(s) whose midpoint is inside the hole region ["
            << holeMin << "," << holeMax << "]^2";
    }
}

TEST_F(HollowTubeTest, HatchesCoverAnnularRegion) {
    MarcSLM::SlmConfig config;
    config.hatch_spacing = 0.3;
    config.hatch_angle   = 0.0;

    auto layers = processor_.sliceAtHeights({1.0f});
    ASSERT_EQ(layers.size(), 1u);

    const auto& layer = layers[0];
    MarcSLM::PathPlanning::HatchGenerator gen(config);

    bool foundValidHatch = false;
    for (const auto& exPoly : layer.exPolygons) {
        if (!exPoly.isValid() || !exPoly.hasHoles()) continue;

        Clipper2Lib::Path64 contourPath;
        for (const auto& pt : exPoly.contour.points) {
            contourPath.emplace_back(
                MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.x)),
                MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.y)));
        }

        Clipper2Lib::Paths64 holePaths;
        for (const auto& hole : exPoly.holes) {
            Clipper2Lib::Path64 holePath;
            for (const auto& pt : hole.points) {
                holePath.emplace_back(
                    MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.x)),
                    MarcSLM::Core::mmToClipperUnits(static_cast<double>(pt.y)));
            }
            if (holePath.size() >= 3) holePaths.push_back(std::move(holePath));
        }

        auto hatchLines = gen.generateHatches(contourPath, holePaths, 0.0);
        ASSERT_FALSE(hatchLines.empty());

        // Verify: hatch lines exist in the annular region
        // (x or y outside [-5,5] but inside [-10,10])
        int insideAnnulus = 0;
        for (const auto& line : hatchLines) {
            float mx = (line.a.x + line.b.x) * 0.5f;
            float my = (line.a.y + line.b.y) * 0.5f;
            bool outsideHole = (mx < -5.5f || mx > 5.5f || my < -5.5f || my > 5.5f);
            bool insideOuter = (mx > -10.5f && mx < 10.5f && my > -10.5f && my < 10.5f);
            if (outsideHole && insideOuter) {
                ++insideAnnulus;
            }
        }
        EXPECT_GT(insideAnnulus, 0)
            << "Hatches must cover the annular region between outer wall and hole";

        foundValidHatch = true;
    }

    EXPECT_TRUE(foundValidHatch)
        << "At least one ExPolygon with a hole must have been hatched";
}

// ==============================================================================
// Diagnostic: Verify TriMesh slicing produces correct loops for hollow tube
// ==============================================================================

TEST_F(HollowTubeTest, DiagnoseSlicingLoops) {
    auto layers = processor_.sliceAtHeights({1.0f});
    ASSERT_EQ(layers.size(), 1u);
    const auto& layer = layers[0];
    // Must have ExPolygons with holes
    ASSERT_FALSE(layer.exPolygons.empty());
    bool foundHole = false;
    for (const auto& ep : layer.exPolygons)
        if (ep.hasHoles()) foundHole = true;
    EXPECT_TRUE(foundHole) << "Hollow tube cross-section must have at least one hole";
}

TEST_F(HollowTubeTest, DiagnoseTriMeshLoops) {
    auto exPolySets = processor_.mesh_->slice({1.0f});
    ASSERT_EQ(exPolySets.size(), 1u);
    ASSERT_FALSE(exPolySets[0].empty());
    bool foundHole = false;
    for (const auto& ep : exPolySets[0])
        if (!ep.holes.empty()) foundHole = true;
    EXPECT_TRUE(foundHole) << "Direct TriMesh slice must produce holes";
}

TEST_F(HollowTubeTest, DiagnoseRawIntersectionLines) {
    auto exPolySets = processor_.mesh_->slice({0.5f, 1.0f, 1.5f});
    ASSERT_EQ(exPolySets.size(), 3u);
    for (const auto& exPolys : exPolySets) {
        bool ok = false;
        for (const auto& ep : exPolys)
            if (!ep.holes.empty() || exPolys.size() >= 2) ok = true;
        EXPECT_TRUE(ok) << "Each slice must carry hole information";
    }
}

TEST_F(HollowTubeTest, DiagnoseInnerWallFacets) {
    EXPECT_TRUE(processor_.mesh_->isManifold())
        << "Hollow tube mesh must be manifold";
    EXPECT_EQ(processor_.mesh_->stats().connectedFacets3Edge,
              processor_.mesh_->facetCount());
}

TEST_F(HollowTubeTest, DiagnoseSliceFacetOutput) {
    auto exPolySets = processor_.mesh_->slice({1.0f});
    ASSERT_EQ(exPolySets.size(), 1u);
    size_t totalPts = 0;
    for (const auto& ep : exPolySets[0]) {
        totalPts += ep.contour.size();
        for (const auto& h : ep.holes) totalPts += h.size();
    }
    // Hollow square tube: 4 outer corners + 4 inner corners = 8 total
    EXPECT_EQ(totalPts, 8u)
        << "Hollow tube cross-section must have 8 total contour+hole points";
}
