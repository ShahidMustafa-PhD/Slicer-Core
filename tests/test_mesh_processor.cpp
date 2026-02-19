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
