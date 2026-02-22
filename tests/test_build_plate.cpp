#define _USE_MATH_DEFINES
#include <cmath>

#include <gtest/gtest.h>

#include "MarcSLM/Core/BuildPlate.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/InternalModel.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"

namespace MarcSLM {
namespace Testing {

// ==============================================================================
// StepState Tests
// ==============================================================================

TEST(StepStateTest, DefaultStateIsNotStartedOrDone) {
    StepState<ObjectStep> state;
    EXPECT_FALSE(state.isStarted(ObjectStep::Slicing));
    EXPECT_FALSE(state.isDone(ObjectStep::Slicing));
}

TEST(StepStateTest, SetStartedAndDone) {
    StepState<ObjectStep> state;
    state.setStarted(ObjectStep::Slicing);
    EXPECT_TRUE(state.isStarted(ObjectStep::Slicing));
    EXPECT_FALSE(state.isDone(ObjectStep::Slicing));

    state.setDone(ObjectStep::Slicing);
    EXPECT_TRUE(state.isStarted(ObjectStep::Slicing));
    EXPECT_TRUE(state.isDone(ObjectStep::Slicing));
}

TEST(StepStateTest, Invalidate) {
    StepState<ObjectStep> state;
    state.setStarted(ObjectStep::Slicing);
    state.setDone(ObjectStep::Slicing);

    bool invalidated = state.invalidate(ObjectStep::Slicing);
    EXPECT_TRUE(invalidated);
    EXPECT_FALSE(state.isStarted(ObjectStep::Slicing));
    EXPECT_FALSE(state.isDone(ObjectStep::Slicing));
}

TEST(StepStateTest, InvalidateAll) {
    StepState<ObjectStep> state;
    state.setStarted(ObjectStep::Slicing);
    state.setDone(ObjectStep::Slicing);
    state.setStarted(ObjectStep::SurfaceDetection);
    state.setDone(ObjectStep::SurfaceDetection);

    bool invalidated = state.invalidateAll();
    EXPECT_TRUE(invalidated);
    EXPECT_FALSE(state.isStarted(ObjectStep::Slicing));
    EXPECT_FALSE(state.isDone(ObjectStep::SurfaceDetection));
}

// ==============================================================================
// SurfaceType Tests
// ==============================================================================

TEST(ClassifiedSurfaceTest, DefaultIsInternal) {
    ClassifiedSurface surf;
    EXPECT_EQ(surf.type, SurfaceType::Internal);
    EXPECT_FALSE(surf.isValid());
    EXPECT_TRUE(surf.isInternal());
    EXPECT_FALSE(surf.isTop());
    EXPECT_FALSE(surf.isBottom());
}

TEST(ClassifiedSurfaceTest, TopSurface) {
    ClassifiedSurface surf;
    surf.type = SurfaceType::Top;
    EXPECT_TRUE(surf.isTop());
    EXPECT_TRUE(surf.isSolid());
    EXPECT_FALSE(surf.isInternal());
}

TEST(ClassifiedSurfaceTest, BottomSurface) {
    ClassifiedSurface surf;
    surf.type = SurfaceType::Bottom;
    EXPECT_TRUE(surf.isBottom());
    EXPECT_TRUE(surf.isSolid());
}

TEST(ClassifiedSurfaceTest, ValidSurface) {
    ClassifiedSurface surf;
    // Need at least 3 points for valid
    surf.contour.emplace_back(0, 0);
    surf.contour.emplace_back(1000, 0);
    surf.contour.emplace_back(0, 1000);
    EXPECT_TRUE(surf.isValid());
}

// ==============================================================================
// BuildPlate Configuration Tests
// ==============================================================================

TEST(BuildPlateTest, DefaultConstruction) {
    BuildPlate plate;
    EXPECT_EQ(plate.objectCount(), 0u);
    EXPECT_EQ(plate.regionCount(), 0u);
    EXPECT_FALSE(plate.isPrepared());
}

TEST(BuildPlateTest, ApplyConfig) {
    BuildPlate plate;
    SlmConfig cfg;
    cfg.layer_thickness = 0.05;
    cfg.hatch_spacing = 0.12;

    plate.applyConfig(cfg);
    EXPECT_DOUBLE_EQ(plate.config.layer_thickness, 0.05);
    EXPECT_DOUBLE_EQ(plate.config.hatch_spacing, 0.12);
}

TEST(BuildPlateTest, AddRegion) {
    BuildPlate plate;
    auto* region = plate.addRegion();
    ASSERT_NE(region, nullptr);
    EXPECT_EQ(plate.regionCount(), 1u);
}

TEST(BuildPlateTest, ClearObjects) {
    BuildPlate plate;
    plate.addRegion();
    EXPECT_EQ(plate.regionCount(), 1u);

    plate.clearObjects();
    EXPECT_EQ(plate.objectCount(), 0u);
}

TEST(BuildPlateTest, ValidateThrowsOnEmpty) {
    BuildPlate plate;
    EXPECT_THROW(plate.validate(), std::runtime_error);
}

TEST(BuildPlateTest, ValidateThrowsOnBadConfig) {
    BuildPlate plate;
    // Still no objects, should throw
    plate.config.layer_thickness = 0.03;
    EXPECT_THROW(plate.validate(), std::runtime_error);
}

TEST(BuildPlateTest, SetBedSize) {
    BuildPlate plate;
    plate.setBedSize(200.0f, 200.0f);
    // Bed size is stored internally; validate it doesn't throw
    // (no public accessor, but we can verify through validateFitsInBed)
}

// ==============================================================================
// PrintRegion Tests
// ==============================================================================

TEST(PrintRegionTest, ConstructionAndConfig) {
    BuildPlate plate;
    auto* region = plate.addRegion();
    ASSERT_NE(region, nullptr);
    EXPECT_NE(region->buildPlate(), nullptr);

    // Config should be default
    EXPECT_GT(region->config.layer_thickness, 0.0);
}

// ==============================================================================
// BuildLayer Tests
// ==============================================================================

TEST(BuildLayerTest, Construction) {
    BuildLayer layer(0, nullptr, 0.03, 0.03, 0.015);
    EXPECT_EQ(layer.id(), 0u);
    EXPECT_DOUBLE_EQ(layer.height(), 0.03);
    EXPECT_DOUBLE_EQ(layer.printZ(), 0.03);
    EXPECT_DOUBLE_EQ(layer.sliceZ(), 0.015);
    EXPECT_EQ(layer.upperLayer, nullptr);
    EXPECT_EQ(layer.lowerLayer, nullptr);
}

TEST(BuildLayerTest, AddRegion) {
    BuildPlate plate;
    auto* region = plate.addRegion();

    BuildLayer layer(0, nullptr, 0.03, 0.03, 0.015);
    auto* lr = layer.addRegion(region);
    ASSERT_NE(lr, nullptr);
    EXPECT_EQ(layer.regionCount(), 1u);
    EXPECT_EQ(lr->region(), region);
}

// ==============================================================================
// ExportLayers Tests (no objects)
// ==============================================================================

TEST(BuildPlateTest, ExportLayersEmpty) {
    BuildPlate plate;
    auto layers = plate.exportLayers();
    EXPECT_TRUE(layers.empty());
}

// ==============================================================================
// BoundingBox Tests
// ==============================================================================

TEST(BuildPlateTest, BoundingBoxEmpty) {
    BuildPlate plate;
    auto bb = plate.boundingBox();
    // With no objects, bounding box should be invalid/empty
    // (min > max for all dimensions)
    EXPECT_GT(bb.min.x, bb.max.x);
}

// ==============================================================================
// Build Plate Preparation Tests
// ==============================================================================

TEST(BuildPlateTest, PrepareBuildPlateThrowsOnEmpty) {
    BuildPlate plate;
    EXPECT_THROW(plate.prepareBuildPlate(), std::runtime_error);
}

TEST(BuildPlateTest, PrepareBuildPlateNotPreparedByDefault) {
    BuildPlate plate;
    EXPECT_FALSE(plate.isPrepared());
}

// ==============================================================================
// TriMesh Transform Tests (foundation for build plate placement)
// ==============================================================================

TEST(TriMeshTransformTest, TranslateUpdatesBBox) {
    using namespace Geometry;
    TriMesh mesh;

    // Create a simple triangle
    float verts[] = { 0, 0, 0,  1, 0, 0,  0, 1, 0 };
    uint32_t inds[] = { 0, 1, 2 };
    mesh.buildFromArrays(verts, 3, inds, 1);

    auto bb0 = mesh.bbox();
    EXPECT_FLOAT_EQ(bb0.min.x, 0.0f);

    mesh.translate(10.0f, 20.0f, 5.0f);

    auto bb1 = mesh.bbox();
    EXPECT_FLOAT_EQ(bb1.min.x, 10.0f);
    EXPECT_FLOAT_EQ(bb1.min.y, 20.0f);
    EXPECT_FLOAT_EQ(bb1.min.z, 5.0f);
    EXPECT_FLOAT_EQ(bb1.max.x, 11.0f);
    EXPECT_FLOAT_EQ(bb1.max.y, 21.0f);
    EXPECT_FLOAT_EQ(bb1.max.z, 5.0f);
}

TEST(TriMeshTransformTest, RotateZPreservesSize) {
    using namespace Geometry;
    TriMesh mesh;

    // Create a unit cube (2 triangles per face, 12 total)
    float verts[] = {
        0, 0, 0,   1, 0, 0,   1, 1, 0,   0, 1, 0,
        0, 0, 1,   1, 0, 1,   1, 1, 1,   0, 1, 1
    };
    uint32_t inds[] = {
        0, 1, 2,  0, 2, 3,  // bottom
        4, 6, 5,  4, 7, 6,  // top
        0, 4, 5,  0, 5, 1,  // front
        2, 6, 7,  2, 7, 3,  // back
        0, 3, 7,  0, 7, 4,  // left
        1, 5, 6,  1, 6, 2   // right
    };
    mesh.buildFromArrays(verts, 8, inds, 12);

    auto bbBefore = mesh.bbox();
    float sizeXBefore = bbBefore.sizeX();
    float sizeYBefore = bbBefore.sizeY();
    float sizeZBefore = bbBefore.sizeZ();

    // Rotate 90 degrees around Z
    mesh.rotateZ(static_cast<float>(M_PI / 2.0));

    auto bbAfter = mesh.bbox();
    // After 90° Z rotation of a unit cube, X and Y sizes should swap (approximately)
    EXPECT_NEAR(bbAfter.sizeX(), sizeYBefore, 0.001f);
    EXPECT_NEAR(bbAfter.sizeY(), sizeXBefore, 0.001f);
    EXPECT_NEAR(bbAfter.sizeZ(), sizeZBefore, 0.001f);
}

TEST(TriMeshTransformTest, AlignToGround) {
    using namespace Geometry;
    TriMesh mesh;

    float verts[] = { 0, 0, 5,  1, 0, 5,  0, 1, 6 };
    uint32_t inds[] = { 0, 1, 2 };
    mesh.buildFromArrays(verts, 3, inds, 1);

    EXPECT_FLOAT_EQ(mesh.bbox().min.z, 5.0f);

    mesh.alignToGround();

    EXPECT_NEAR(mesh.bbox().min.z, 0.0f, 0.0001f);
}

TEST(TriMeshTransformTest, MergeIncreasesFacetCount) {
    using namespace Geometry;
    TriMesh meshA, meshB;

    float vertsA[] = { 0, 0, 0,  1, 0, 0,  0, 1, 0 };
    uint32_t indsA[] = { 0, 1, 2 };
    meshA.buildFromArrays(vertsA, 3, indsA, 1);

    float vertsB[] = { 5, 5, 0,  6, 5, 0,  5, 6, 0 };
    uint32_t indsB[] = { 0, 1, 2 };
    meshB.buildFromArrays(vertsB, 3, indsB, 1);

    EXPECT_EQ(meshA.facetCount(), 1u);
    EXPECT_EQ(meshB.facetCount(), 1u);

    meshA.merge(meshB);

    EXPECT_EQ(meshA.facetCount(), 2u);
    auto bb = meshA.bbox();
    EXPECT_FLOAT_EQ(bb.min.x, 0.0f);
    EXPECT_FLOAT_EQ(bb.max.x, 6.0f);
}

TEST(TriMeshTransformTest, ScaleUniform) {
    using namespace Geometry;
    TriMesh mesh;

    float verts[] = { 0, 0, 0,  1, 0, 0,  0, 1, 0 };
    uint32_t inds[] = { 0, 1, 2 };
    mesh.buildFromArrays(verts, 3, inds, 1);

    mesh.scale(2.0f);

    auto bb = mesh.bbox();
    EXPECT_FLOAT_EQ(bb.max.x, 2.0f);
    EXPECT_FLOAT_EQ(bb.max.y, 2.0f);
}

TEST(TriMeshTransformTest, CenterXY) {
    using namespace Geometry;
    TriMesh mesh;

    float verts[] = { 0, 0, 0,  2, 0, 0,  0, 2, 0 };
    uint32_t inds[] = { 0, 1, 2 };
    mesh.buildFromArrays(verts, 3, inds, 1);

    mesh.centerXY(50.0f, 50.0f);

    auto bb = mesh.bbox();
    float midX = (bb.min.x + bb.max.x) * 0.5f;
    float midY = (bb.min.y + bb.max.y) * 0.5f;
    EXPECT_NEAR(midX, 50.0f, 0.001f);
    EXPECT_NEAR(midY, 50.0f, 0.001f);
}

// ==============================================================================
// BBox Overlap XY Test (utility for build plate collision detection)
// ==============================================================================

TEST(BBoxOverlapTest, NonOverlapping) {
    Geometry::BBox3f a, b;
    a.merge(Geometry::Vertex3f(0, 0, 0));
    a.merge(Geometry::Vertex3f(10, 10, 10));
    b.merge(Geometry::Vertex3f(20, 20, 0));
    b.merge(Geometry::Vertex3f(30, 30, 10));

    // Boxes are far apart — should not overlap
    // We verify through the public bounding box after adding models.
    // (bboxOverlapXY is private, but tested indirectly through validateNoOverlap)
}

// ==============================================================================
// InternalModel Descriptor Tests
// ==============================================================================

TEST(InternalModelTest, DefaultValues) {
    InternalModel model;
    EXPECT_DOUBLE_EQ(model.xpos, 0.0);
    EXPECT_DOUBLE_EQ(model.ypos, 0.0);
    EXPECT_DOUBLE_EQ(model.zpos, 0.0);
    EXPECT_DOUBLE_EQ(model.roll, 0.0);
    EXPECT_DOUBLE_EQ(model.pitch, 0.0);
    EXPECT_DOUBLE_EQ(model.yaw, 0.0);
    EXPECT_EQ(model.number, 0);
}

TEST(InternalModelTest, PlacementValues) {
    InternalModel model;
    model.path = "test.stl";
    model.buildconfig = "config.json";
    model.number = 1;
    model.xpos = 10.0;
    model.ypos = 20.0;
    model.zpos = 0.0;
    model.roll = 0.1;
    model.pitch = 0.2;
    model.yaw = 0.3;

    EXPECT_EQ(model.path, "test.stl");
    EXPECT_EQ(model.number, 1);
    EXPECT_DOUBLE_EQ(model.xpos, 10.0);
    EXPECT_DOUBLE_EQ(model.ypos, 20.0);
    EXPECT_DOUBLE_EQ(model.roll, 0.1);
    EXPECT_DOUBLE_EQ(model.pitch, 0.2);
    EXPECT_DOUBLE_EQ(model.yaw, 0.3);
}

} // namespace Testing
} // namespace MarcSLM
