// ==============================================================================
// MarcSLM - Mesh I/O Unit Tests
// ==============================================================================
// Comprehensive test suite for MeshIO and MeshData classes.
// ==============================================================================

#include <gtest/gtest.h>
#include "MarcSLM/IO/MeshIO.hpp"
#include "MarcSLM/IO/MeshData.hpp"

#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace MarcSLM {
namespace IO {
namespace Test {

// ==============================================================================
// Test Fixtures
// ==============================================================================

class MeshDataTest : public ::testing::Test {
protected:
    MeshData mesh;
};

class MeshIOTest : public ::testing::Test {
protected:
    MeshIO loader;
    MeshData mesh;
    
    // Helper: Create a temporary STL file for testing
    std::string createTestSTL(const std::string& name) {
        std::string filename = "test_" + name + ".stl";
        std::ofstream file(filename, std::ios::binary);
        
        if (!file) {
            return "";
        }
        
        // Write minimal binary STL (1 triangle)
        char header[80] = {0};
        std::memcpy(header, "Test STL", 8);
        file.write(header, 80);
        
        uint32_t numTriangles = 1;
        file.write(reinterpret_cast<const char*>(&numTriangles), 4);
        
        // Triangle: normal + 3 vertices + attribute
        float triangle[12] = {
            0.0f, 0.0f, 1.0f,       // Normal
            0.0f, 0.0f, 0.0f,       // Vertex 0
            1.0f, 0.0f, 0.0f,       // Vertex 1
            0.0f, 1.0f, 0.0f        // Vertex 2
        };
        file.write(reinterpret_cast<const char*>(triangle), 12 * sizeof(float));
        
        uint16_t attribute = 0;
        file.write(reinterpret_cast<const char*>(&attribute), 2);
        
        file.close();
        return filename;
    }
    
    void TearDown() override {
        // Cleanup test files
        for (const auto& entry : fs::directory_iterator(".")) {
            if (entry.path().filename().string().find("test_") == 0) {
                fs::remove(entry.path());
            }
        }
    }
};

// ==============================================================================
// MeshData Tests
// ==============================================================================

TEST_F(MeshDataTest, DefaultConstruction) {
    EXPECT_TRUE(mesh.isEmpty());
    EXPECT_EQ(mesh.vertexCount(), 0);
    EXPECT_EQ(mesh.faceCount(), 0);
    EXPECT_TRUE(mesh.sourcePath.empty());
    EXPECT_TRUE(mesh.formatHint.empty());
}

TEST_F(MeshDataTest, IsEmpty) {
    EXPECT_TRUE(mesh.isEmpty());
    
    mesh.V.resize(1, 3);
    mesh.F.resize(1, 3);
    EXPECT_FALSE(mesh.isEmpty());
    
    mesh.clear();
    EXPECT_TRUE(mesh.isEmpty());
}

TEST_F(MeshDataTest, VertexFaceCount) {
    mesh.V.resize(100, 3);
    mesh.F.resize(200, 3);
    
    EXPECT_EQ(mesh.vertexCount(), 100);
    EXPECT_EQ(mesh.faceCount(), 200);
}

TEST_F(MeshDataTest, BoundingBox) {
    // Create a simple cube mesh
    mesh.V.resize(8, 3);
    mesh.V << 0, 0, 0,
              1, 0, 0,
              1, 1, 0,
              0, 1, 0,
              0, 0, 1,
              1, 0, 1,
              1, 1, 1,
              0, 1, 1;
    
    mesh.F.resize(1, 3);  // At least one face
    mesh.F << 0, 1, 2;
    
    auto bbox = mesh.boundingBox();
    
    EXPECT_DOUBLE_EQ(bbox(0), 0.0);  // minX
    EXPECT_DOUBLE_EQ(bbox(1), 0.0);  // minY
    EXPECT_DOUBLE_EQ(bbox(2), 0.0);  // minZ
    EXPECT_DOUBLE_EQ(bbox(3), 1.0);  // maxX
    EXPECT_DOUBLE_EQ(bbox(4), 1.0);  // maxY
    EXPECT_DOUBLE_EQ(bbox(5), 1.0);  // maxZ
}

TEST_F(MeshDataTest, Dimensions) {
    mesh.V.resize(2, 3);
    mesh.V << 0, 0, 0,
              10, 20, 30;
    mesh.F.resize(1, 3);
    mesh.F << 0, 0, 1;
    
    auto dims = mesh.dimensions();
    
    EXPECT_DOUBLE_EQ(dims(0), 10.0);  // Width
    EXPECT_DOUBLE_EQ(dims(1), 20.0);  // Depth
    EXPECT_DOUBLE_EQ(dims(2), 30.0);  // Height
}

TEST_F(MeshDataTest, BoundingBoxEmpty) {
    auto bbox = mesh.boundingBox();
    EXPECT_TRUE(bbox.isZero());
}

TEST_F(MeshDataTest, DimensionsEmpty) {
    auto dims = mesh.dimensions();
    EXPECT_TRUE(dims.isZero());
}

TEST_F(MeshDataTest, Clear) {
    mesh.V.resize(10, 3);
    mesh.F.resize(20, 3);
    mesh.N.resize(20, 3);
    mesh.sourcePath = "test.stl";
    mesh.formatHint = "stl";
    
    mesh.clear();
    
    EXPECT_TRUE(mesh.isEmpty());
    EXPECT_EQ(mesh.V.rows(), 0);
    EXPECT_EQ(mesh.F.rows(), 0);
    EXPECT_EQ(mesh.N.rows(), 0);
    EXPECT_TRUE(mesh.sourcePath.empty());
    EXPECT_TRUE(mesh.formatHint.empty());
}

TEST_F(MeshDataTest, Reserve) {
    mesh.reserve(100, 200);
    
    EXPECT_EQ(mesh.V.rows(), 100);
    EXPECT_EQ(mesh.V.cols(), 3);
    EXPECT_EQ(mesh.F.rows(), 200);
    EXPECT_EQ(mesh.F.cols(), 3);
    EXPECT_EQ(mesh.N.rows(), 200);
    EXPECT_EQ(mesh.N.cols(), 3);
}

// ==============================================================================
// MeshIO: Format Support Tests
// ==============================================================================

TEST(MeshIOStaticTest, SupportedFormats) {
    auto formats = MeshIO::supportedFormats();
    
    EXPECT_FALSE(formats.empty());
    EXPECT_GE(formats.size(), 3);  // At least STL, OBJ, AMF
    
    // Check for expected formats
    EXPECT_NE(std::find(formats.begin(), formats.end(), ".stl"), formats.end());
    EXPECT_NE(std::find(formats.begin(), formats.end(), ".obj"), formats.end());
    EXPECT_NE(std::find(formats.begin(), formats.end(), ".amf"), formats.end());
}

TEST(MeshIOStaticTest, IsSupportedFormat) {
    EXPECT_TRUE(MeshIO::isSupportedFormat(".stl"));
    EXPECT_TRUE(MeshIO::isSupportedFormat(".STL"));  // Case-insensitive
    EXPECT_TRUE(MeshIO::isSupportedFormat(".obj"));
    EXPECT_TRUE(MeshIO::isSupportedFormat(".amf"));
    
    EXPECT_FALSE(MeshIO::isSupportedFormat(".txt"));
    EXPECT_FALSE(MeshIO::isSupportedFormat(".xyz"));
    EXPECT_FALSE(MeshIO::isSupportedFormat(""));
}

// ==============================================================================
// MeshIO: Error Handling Tests
// ==============================================================================

TEST_F(MeshIOTest, LoadNonExistentFile) {
    EXPECT_THROW(
        loader.loadFromFile("nonexistent_file.stl", mesh),
        MeshLoadError
    );
    
    EXPECT_FALSE(loader.lastError().empty());
}

TEST_F(MeshIOTest, LoadUnsupportedFormat) {
    // Create a dummy file with unsupported extension
    std::ofstream file("test_unsupported.xyz");
    file << "dummy content";
    file.close();
    
    EXPECT_THROW(
        loader.loadFromFile("test_unsupported.xyz", mesh),
        MeshLoadError
    );
}

TEST_F(MeshIOTest, LoadEmptyFile) {
    // Create an empty STL file
    std::ofstream file("test_empty.stl");
    file.close();
    
    EXPECT_THROW(
        loader.loadFromFile("test_empty.stl", mesh),
        MeshLoadError
    );
}

// ==============================================================================
// MeshIO: Loading Tests
// ==============================================================================

TEST_F(MeshIOTest, LoadValidSTL) {
    std::string filename = createTestSTL("valid");
    ASSERT_FALSE(filename.empty());
    
    EXPECT_NO_THROW(
        loader.loadFromFile(filename, mesh)
    );
    
    EXPECT_FALSE(mesh.isEmpty());
    EXPECT_EQ(mesh.vertexCount(), 3);  // 1 triangle = 3 unique vertices
    EXPECT_EQ(mesh.faceCount(), 1);
    EXPECT_EQ(mesh.formatHint, "stl");
}

TEST_F(MeshIOTest, MeshDataPopulated) {
    std::string filename = createTestSTL("populated");
    ASSERT_FALSE(filename.empty());
    
    loader.loadFromFile(filename, mesh);
    
    // Check vertex matrix dimensions
    EXPECT_EQ(mesh.V.rows(), 3);
    EXPECT_EQ(mesh.V.cols(), 3);
    
    // Check face matrix dimensions
    EXPECT_EQ(mesh.F.rows(), 1);
    EXPECT_EQ(mesh.F.cols(), 3);
    
    // Check normal matrix dimensions
    EXPECT_EQ(mesh.N.rows(), 1);
    EXPECT_EQ(mesh.N.cols(), 3);
}

TEST_F(MeshIOTest, FaceNormalsComputed) {
    std::string filename = createTestSTL("normals");
    ASSERT_FALSE(filename.empty());
    
    loader.loadFromFile(filename, mesh);
    
    ASSERT_EQ(mesh.N.rows(), 1);
    
    // Normal should be a unit vector
    Eigen::Vector3d normal = mesh.N.row(0);
    double length = normal.norm();
    EXPECT_NEAR(length, 1.0, 1e-6);
}

TEST_F(MeshIOTest, SourcePathSet) {
    std::string filename = createTestSTL("path");
    ASSERT_FALSE(filename.empty());
    
    loader.loadFromFile(filename, mesh);
    
    EXPECT_EQ(mesh.sourcePath, filename);
}

TEST_F(MeshIOTest, ClearBeforeLoad) {
    std::string filename = createTestSTL("clear");
    ASSERT_FALSE(filename.empty());
    
    // Pre-populate mesh with dummy data
    mesh.V.resize(100, 3);
    mesh.F.resize(200, 3);
    mesh.sourcePath = "old_file.stl";
    
    loader.loadFromFile(filename, mesh);
    
    // Should be cleared and repopulated
    EXPECT_EQ(mesh.vertexCount(), 3);
    EXPECT_EQ(mesh.faceCount(), 1);
    EXPECT_EQ(mesh.sourcePath, filename);
}

// ==============================================================================
// MeshIO: Exception Tests
// ==============================================================================

TEST_F(MeshIOTest, MeshLoadErrorException) {
    try {
        loader.loadFromFile("nonexistent.stl", mesh);
        FAIL() << "Expected MeshLoadError";
    } catch (const MeshLoadError& e) {
        EXPECT_EQ(e.filePath(), "nonexistent.stl");
        EXPECT_FALSE(e.reason().empty());
    }
}

TEST_F(MeshIOTest, MeshIOErrorHierarchy) {
    try {
        loader.loadFromFile("nonexistent.stl", mesh);
        FAIL() << "Expected exception";
    } catch (const MeshLoadError& e) {
        // Can catch as MeshLoadError
        SUCCEED();
    } catch (const MeshIOError& e) {
        // Can also catch as base MeshIOError
        SUCCEED();
    }
}

// ==============================================================================
// MeshIO: Manifold Validation Tests
// ==============================================================================

TEST_F(MeshIOTest, ValidateEmptyMesh) {
    EXPECT_THROW(
        loader.validateManifold(mesh),
        MeshValidationError
    );
}

TEST_F(MeshIOTest, ValidateManifoldMesh) {
    // Create a simple manifold cube (6 faces, 12 triangles)
    mesh.V.resize(8, 3);
    mesh.V << 0, 0, 0,
              1, 0, 0,
              1, 1, 0,
              0, 1, 0,
              0, 0, 1,
              1, 0, 1,
              1, 1, 1,
              0, 1, 1;
    
    // 12 triangles forming a closed cube
    mesh.F.resize(12, 3);
    mesh.F << 0, 1, 2,  // Bottom face
              0, 2, 3,
              4, 5, 6,  // Top face
              4, 6, 7,
              0, 1, 5,  // Front face
              0, 5, 4,
              2, 3, 7,  // Back face
              2, 7, 6,
              0, 3, 7,  // Left face
              0, 7, 4,
              1, 2, 6,  // Right face
              1, 6, 5;
    
    EXPECT_NO_THROW(loader.validateManifold(mesh));
}

TEST_F(MeshIOTest, DetectNonManifoldOpenEdge) {
    // Create mesh with one open edge (non-manifold)
    mesh.V.resize(4, 3);
    mesh.V << 0, 0, 0,
              1, 0, 0,
              0.5, 1, 0,
              0.5, 0, 1;
    
    // Only 2 triangles (edge 0-1 shared by only 1 face)
    mesh.F.resize(2, 3);
    mesh.F << 0, 1, 2,
              0, 1, 3;
    
    EXPECT_THROW(
        loader.validateManifold(mesh),
        NonManifoldMeshError
    );
}

// ==============================================================================
// Performance Sanity Tests
// ==============================================================================

TEST_F(MeshIOTest, LoadPerformanceSanityCheck) {
    std::string filename = createTestSTL("performance");
    ASSERT_FALSE(filename.empty());
    
    auto start = std::chrono::high_resolution_clock::now();
    loader.loadFromFile(filename, mesh);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should load a 1-triangle mesh in under 100ms
    EXPECT_LT(duration.count(), 100);
}

} // namespace Test
} // namespace IO
} // namespace MarcSLM
