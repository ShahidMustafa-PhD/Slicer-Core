// ==============================================================================
// MarcSLM - Mesh Processor Unit Tests
// ==============================================================================
// Comprehensive tests for the MeshProcessor slicing engine
// ==============================================================================

#include <MarcSLM/Geometry/MeshProcessor.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>

using namespace MarcSLM::Geometry;

namespace {

// Test fixture for MeshProcessor tests
class MeshProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor_ = std::make_unique<MeshProcessor>();
    }

    void TearDown() override {
        processor_.reset();
    }

    // Helper to create a simple test mesh (cube)
    manifold::MeshGL createCubeMesh() {
        manifold::MeshGL mesh;

        // Cube vertices (8 corners)
        const std::vector<float> verts = {
            // Bottom face
            -1.0f, -1.0f, -1.0f,   // 0
             1.0f, -1.0f, -1.0f,   // 1
             1.0f,  1.0f, -1.0f,   // 2
            -1.0f,  1.0f, -1.0f,   // 3
            // Top face
            -1.0f, -1.0f,  1.0f,   // 4
             1.0f, -1.0f,  1.0f,   // 5
             1.0f,  1.0f,  1.0f,   // 6
            -1.0f,  1.0f,  1.0f    // 7
        };

        // Cube triangles (12 triangles, 36 indices)
        const std::vector<uint32_t> tris = {
            // Bottom face
            0, 1, 2,   0, 2, 3,
            // Top face
            4, 5, 6,   4, 6, 7,
            // Front face
            0, 1, 5,   0, 5, 4,
            // Back face
            3, 2, 6,   3, 6, 7,
            // Left face
            0, 3, 7,   0, 7, 4,
            // Right face
            1, 2, 6,   1, 6, 5
        };

        mesh.vertProperties = verts;
        mesh.triVerts = tris;
        mesh.numProp = 3;

        return mesh;
    }

    std::unique_ptr<MeshProcessor> processor_;
};

// Test fixture for tests requiring a loaded mesh
class MeshProcessorWithCubeTest : public MeshProcessorTest {
protected:
    void SetUp() override {
        MeshProcessorTest::SetUp();

        // Create and load a cube mesh
        manifold::MeshGL cubeMesh = createCubeMesh();
        manifold::Manifold cube(cubeMesh);

        // Simulate loading by setting the mesh directly
        processor_->mesh_ = std::make_unique<manifold::Manifold>(cubeMesh);
        processor_->bbox_ = processor_->mesh_->BoundingBox();
    }
};

} // anonymous namespace

// ==============================================================================
// Basic Construction and State Tests
// ==============================================================================

TEST_F(MeshProcessorTest, DefaultConstruction) {
    EXPECT_FALSE(processor_->hasValidMesh());
}

TEST_F(MeshProcessorTest, MoveOperations) {
    auto processor2 = std::move(*processor_);
    EXPECT_FALSE(processor_->hasValidMesh());  // Moved from should be empty
}

// ==============================================================================
// Mesh Loading Tests
// ==============================================================================

TEST_F(MeshProcessorTest, LoadMesh_InvalidFile) {
    EXPECT_THROW(processor_->loadMesh("nonexistent.stl"), MeshLoadError);
}

TEST_F(MeshProcessorTest, LoadMesh_EmptyMesh) {
    // Create an empty Assimp scene (would need actual file I/O to test properly)
    // For now, just test that the method exists and has proper error handling
    EXPECT_THROW(processor_->loadMesh(""), MeshLoadError);
}

// ==============================================================================
// Cube Mesh Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, HasValidMesh) {
    EXPECT_TRUE(processor_->hasValidMesh());
}

TEST_F(MeshProcessorWithCubeTest, GetBoundingBox) {
    manifold::Box bbox = processor_->getBoundingBox();

    // Cube from (-1,-1,-1) to (1,1,1)
    EXPECT_FLOAT_EQ(bbox.min.x, -1.0f);
    EXPECT_FLOAT_EQ(bbox.min.y, -1.0f);
    EXPECT_FLOAT_EQ(bbox.min.z, -1.0f);
    EXPECT_FLOAT_EQ(bbox.max.x,  1.0f);
    EXPECT_FLOAT_EQ(bbox.max.y,  1.0f);
    EXPECT_FLOAT_EQ(bbox.max.z,  1.0f);
}

TEST_F(MeshProcessorWithCubeTest, GetMeshStats) {
    auto [verts, tris, volume] = processor_->getMeshStats();

    EXPECT_EQ(verts, 8u);   // Cube has 8 vertices
    EXPECT_EQ(tris, 12u);   // Cube has 12 triangles
    EXPECT_NEAR(volume, 8.0, 1e-6);  // Cube volume = 2^3 = 8
}

// ==============================================================================
// Slicing Parameter Validation Tests
// ==============================================================================

TEST_F(MeshProcessorTest, SliceUniform_InvalidThickness) {
    EXPECT_THROW(processor_->sliceUniform(0.0f), SlicingError);
    EXPECT_THROW(processor_->sliceUniform(-1.0f), SlicingError);
    EXPECT_THROW(processor_->sliceUniform(0.0001f), SlicingError);  // Too small
    EXPECT_THROW(processor_->sliceUniform(100.0f), SlicingError);   // Too large
}

TEST_F(MeshProcessorTest, SliceAdaptive_InvalidParams) {
    EXPECT_THROW(processor_->sliceAdaptive(0.1f, 0.05f, 0.1f), SlicingError);  // min > max
    EXPECT_THROW(processor_->sliceAdaptive(-0.1f, 0.1f, 0.1f), SlicingError);  // negative
    EXPECT_THROW(processor_->sliceAdaptive(0.1f, 0.2f, -0.1f), SlicingError);  // negative error
    EXPECT_THROW(processor_->sliceAdaptive(0.1f, 0.2f, 1.5f), SlicingError);   // error > 1
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

    // Cube height is 2.0, layer thickness 0.5 ? 4 layers
    ASSERT_EQ(layers.size(), 4u);

    // Check layer metadata
    for (size_t i = 0; i < layers.size(); ++i) {
        EXPECT_EQ(layers[i].layerNumber, i);
        EXPECT_FLOAT_EQ(layers[i].layerThickness, 0.5f);
        EXPECT_FLOAT_EQ(layers[i].layerHeight, -1.0f + i * 0.5f);
    }

    // Cube should have square cross-sections
    // Each layer should have polylines representing the contours
    for (const auto& layer : layers) {
        // Cube cross-section should be a square (4 points)
        if (!layer.polylines.empty()) {
            EXPECT_GE(layer.polylines[0].points.size(), 4u);
        }
    }
}

TEST_F(MeshProcessorWithCubeTest, SliceUniform_ThickLayers) {
    LayerStack layers = processor_->sliceUniform(2.5f);  // Thicker than cube

    // Should still produce layers
    EXPECT_GE(layers.size(), 1u);

    // First layer should cover the entire cube
    EXPECT_FLOAT_EQ(layers[0].layerHeight, -1.0f);
    EXPECT_FLOAT_EQ(layers[0].layerThickness, 2.5f);
}

// ==============================================================================
// Adaptive Slicing Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, SliceAdaptive_Cube) {
    LayerStack layers = processor_->sliceAdaptive(0.01f, 0.1f, 0.05f);

    // Should produce multiple layers
    EXPECT_GE(layers.size(), 1u);

    // All layers should have valid heights
    for (const auto& layer : layers) {
        EXPECT_GE(layer.layerThickness, 0.01f);
        EXPECT_LE(layer.layerThickness, 0.1f);
        EXPECT_GE(layer.layerHeight, -1.0f);
        EXPECT_LE(layer.layerHeight, 1.0f);
    }

    // Total height should cover the cube
    if (!layers.empty()) {
        float totalHeight = layers.back().layerHeight + layers.back().layerThickness;
        EXPECT_GE(totalHeight, 1.0f);  // Should reach top of cube
    }
}

TEST_F(MeshProcessorWithCubeTest, SliceAdaptive_MinEqualsMax) {
    LayerStack layers = processor_->sliceAdaptive(0.05f, 0.05f, 0.05f);

    // Should behave like uniform slicing
    EXPECT_GE(layers.size(), 1u);

    // All layers should have the same thickness
    for (const auto& layer : layers) {
        EXPECT_FLOAT_EQ(layer.layerThickness, 0.05f);
    }
}

// ==============================================================================
// Single Height Slicing Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, SliceAtHeight_CubeCenter) {
    LayerSlices layer = processor_->sliceAtHeight(0.0f);

    EXPECT_EQ(layer.layerNumber, 0u);
    EXPECT_FLOAT_EQ(layer.layerHeight, 0.0f);
    EXPECT_FLOAT_EQ(layer.layerThickness, 0.0f);

    // Should have polylines representing the square cross-section
    EXPECT_FALSE(layer.polylines.empty());
    if (!layer.polylines.empty()) {
        // Square should have at least 4 points
        EXPECT_GE(layer.polylines[0].points.size(), 4u);
    }
}

TEST_F(MeshProcessorWithCubeTest, SliceAtHeight_OutsideBounds) {
    // Slice above the cube
    LayerSlices layer = processor_->sliceAtHeight(2.0f);

    // Should be empty (no intersection)
    EXPECT_TRUE(layer.polylines.empty());
}

TEST_F(MeshProcessorWithCubeTest, SliceAtHeights_Multiple) {
    std::vector<float> heights = {-0.5f, 0.0f, 0.5f};
    LayerStack layers = processor_->sliceAtHeights(heights);

    ASSERT_EQ(layers.size(), 3u);

    for (size_t i = 0; i < layers.size(); ++i) {
        EXPECT_EQ(layers[i].layerNumber, i);
        EXPECT_FLOAT_EQ(layers[i].layerHeight, heights[i]);
        EXPECT_FLOAT_EQ(layers[i].layerThickness, 0.0f);
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
// Performance Tests (Basic Sanity Checks)
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, Performance_UniformSlicing) {
    // Basic performance sanity check
    auto start = std::chrono::high_resolution_clock::now();

    LayerStack layers = processor_->sliceUniform(0.1f);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in reasonable time (< 1 second for a simple cube)
    EXPECT_LT(duration.count(), 1000);

    // Should produce reasonable number of layers
    EXPECT_GE(layers.size(), 10u);  // Cube height 2.0 / 0.1 = 20 layers
    EXPECT_LE(layers.size(), 30u);  // But not too many
}

// ==============================================================================
// Integration Tests
// ==============================================================================

TEST_F(MeshProcessorWithCubeTest, Integration_FullWorkflow) {
    // Load mesh (already done in SetUp)
    ASSERT_TRUE(processor_->hasValidMesh());

    // Get stats
    auto [verts, tris, volume] = processor_->getMeshStats();
    EXPECT_EQ(verts, 8u);
    EXPECT_EQ(tris, 12u);
    EXPECT_NEAR(volume, 8.0, 1e-6);

    // Uniform slicing
    LayerStack uniformLayers = processor_->sliceUniform(0.2f);
    EXPECT_GE(uniformLayers.size(), 5u);

    // Adaptive slicing
    LayerStack adaptiveLayers = processor_->sliceAdaptive(0.01f, 0.2f);
    EXPECT_GE(adaptiveLayers.size(), 1u);

    // Single height slicing
    LayerSlices singleLayer = processor_->sliceAtHeight(0.0f);
    EXPECT_FALSE(singleLayer.polylines.empty());

    // Verify layer data integrity
    for (const auto& layer : uniformLayers) {
        EXPECT_GE(layer.layerNumber, 0u);
        EXPECT_GE(layer.layerHeight, -1.0f);
        EXPECT_LE(layer.layerHeight, 1.0f);
        EXPECT_FLOAT_EQ(layer.layerThickness, 0.2f);
    }
}
