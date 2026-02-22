// ==============================================================================
// MarcSLM - Build Plate Preparation Tests
// ==============================================================================
// Tests for the build plate preparation pipeline:
//   - ModelPlacement value type
//   - BuildPlatePreparator service class
//   - Integration with BuildPlate orchestrator
// ==============================================================================

#define _USE_MATH_DEFINES
#include <cmath>

#include <gtest/gtest.h>

#include "MarcSLM/Core/BuildPlate.hpp"
#include "MarcSLM/Core/BuildPlate/ModelPlacement.hpp"
#include "MarcSLM/Core/BuildPlate/BuildPlatePreparator.hpp"
#include "MarcSLM/Core/InternalModel.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Geometry/MeshProcessor.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"

#include <memory>
#include <vector>

namespace MarcSLM {
namespace Testing {

// ==============================================================================
// Helper: Create a simple box mesh processor for testing
// ==============================================================================

/// Creates a MeshProcessor with a unit cube [0,1]^3 (12 triangles).
static std::unique_ptr<Geometry::MeshProcessor> makeBoxProcessor(
    float sizeX = 10.0f, float sizeY = 10.0f, float sizeZ = 10.0f)
{
    auto proc = std::make_unique<Geometry::MeshProcessor>();

    // Build a box mesh directly via TriMesh
    auto mesh = std::make_unique<Geometry::TriMesh>();

    // 8 vertices
    float verts[] = {
        0, 0, 0,          sizeX, 0, 0,
        sizeX, sizeY, 0,  0, sizeY, 0,
        0, 0, sizeZ,      sizeX, 0, sizeZ,
        sizeX, sizeY, sizeZ, 0, sizeY, sizeZ
    };
    // 12 triangles (2 per face)
    uint32_t inds[] = {
        0, 1, 2,  0, 2, 3,  // bottom
        4, 6, 5,  4, 7, 6,  // top
        0, 4, 5,  0, 5, 1,  // front
        2, 6, 7,  2, 7, 3,  // back
        0, 3, 7,  0, 7, 4,  // left
        1, 5, 6,  1, 6, 2   // right
    };

    mesh->buildFromArrays(verts, 8, inds, 12);
    proc->mesh_ = std::move(mesh);
    proc->bbox_ = proc->mesh_->bbox();

    return proc;
}

// ==============================================================================
// ModelPlacement Tests
// ==============================================================================

TEST(ModelPlacementTest, DefaultConstruction) {
    BP::ModelPlacement pl;
    EXPECT_EQ(pl.modelId, 0u);
    EXPECT_DOUBLE_EQ(pl.x, 0.0);
    EXPECT_DOUBLE_EQ(pl.y, 0.0);
    EXPECT_DOUBLE_EQ(pl.z, 0.0);
    EXPECT_DOUBLE_EQ(pl.roll, 0.0);
    EXPECT_DOUBLE_EQ(pl.pitch, 0.0);
    EXPECT_DOUBLE_EQ(pl.yaw, 0.0);
    EXPECT_FALSE(pl.hasTransform());
    EXPECT_FALSE(pl.hasRotation());
    EXPECT_FALSE(pl.hasTranslation());
}

TEST(ModelPlacementTest, ConstructFromInternalModel) {
    InternalModel im;
    im.path = "test.stl";
    im.number = 5;
    im.xpos = 10.0;
    im.ypos = 20.0;
    im.zpos = 0.0;
    im.roll = 0.1;
    im.pitch = 0.0;
    im.yaw = 0.3;

    BP::ModelPlacement pl(im);
    EXPECT_EQ(pl.modelId, 5u);
    EXPECT_EQ(pl.modelPath, "test.stl");
    EXPECT_DOUBLE_EQ(pl.x, 10.0);
    EXPECT_DOUBLE_EQ(pl.y, 20.0);
    EXPECT_DOUBLE_EQ(pl.z, 0.0);
    EXPECT_DOUBLE_EQ(pl.roll, 0.1);
    EXPECT_DOUBLE_EQ(pl.yaw, 0.3);
    EXPECT_TRUE(pl.hasTransform());
    EXPECT_TRUE(pl.hasRotation());
    EXPECT_TRUE(pl.hasTranslation());
}

TEST(ModelPlacementTest, HasRotationDetection) {
    BP::ModelPlacement pl;
    EXPECT_FALSE(pl.hasRotation());

    pl.yaw = 0.5;
    EXPECT_TRUE(pl.hasRotation());

    pl.yaw = 0.0;
    pl.roll = 0.01;
    EXPECT_TRUE(pl.hasRotation());
}

TEST(ModelPlacementTest, HasTranslationDetection) {
    BP::ModelPlacement pl;
    EXPECT_FALSE(pl.hasTranslation());

    pl.x = 5.0;
    EXPECT_TRUE(pl.hasTranslation());
}

TEST(ModelPlacementTest, EstimateBBox) {
    // A box [0,10] x [0,10] x [0,10] with no transform
    Geometry::BBox3f src;
    src.merge(Geometry::Vertex3f(0, 0, 0));
    src.merge(Geometry::Vertex3f(10, 10, 10));

    BP::ModelPlacement pl;
    pl.x = 20.0;
    pl.y = 30.0;

    auto bb = pl.estimateTransformedBBox(src);
    // After translation the box should be at [20,30,0]-[30,40,10]
    EXPECT_NEAR(bb.min.x, 20.0f, 0.01f);
    EXPECT_NEAR(bb.min.y, 30.0f, 0.01f);
    EXPECT_NEAR(bb.min.z, 0.0f, 0.01f);
    EXPECT_NEAR(bb.max.x, 30.0f, 0.01f);
    EXPECT_NEAR(bb.max.y, 40.0f, 0.01f);
    EXPECT_NEAR(bb.max.z, 10.0f, 0.01f);
}

TEST(ModelPlacementTest, ApplyToMeshProcessor) {
    auto proc = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    ASSERT_TRUE(proc->hasValidMesh());

    BP::ModelPlacement pl;
    pl.x = 50.0;
    pl.y = 100.0;

    pl.applyTo(*proc);

    auto bb = proc->getBoundingBox();
    EXPECT_NEAR(bb.min.x, 50.0f, 0.1f);
    EXPECT_NEAR(bb.min.y, 100.0f, 0.1f);
    EXPECT_NEAR(bb.min.z, 0.0f, 0.1f);
}

// ==============================================================================
// BuildPlatePreparator Tests
// ==============================================================================

TEST(BuildPlatePreparatorTest, BBoxOverlapXY_NonOverlapping) {
    Geometry::BBox3f a, b;
    a.merge(Geometry::Vertex3f(0, 0, 0));
    a.merge(Geometry::Vertex3f(10, 10, 10));
    b.merge(Geometry::Vertex3f(20, 20, 0));
    b.merge(Geometry::Vertex3f(30, 30, 10));

    EXPECT_FALSE(BP::BuildPlatePreparator::bboxOverlapXY(a, b));
}

TEST(BuildPlatePreparatorTest, BBoxOverlapXY_Overlapping) {
    Geometry::BBox3f a, b;
    a.merge(Geometry::Vertex3f(0, 0, 0));
    a.merge(Geometry::Vertex3f(15, 15, 10));
    b.merge(Geometry::Vertex3f(10, 10, 0));
    b.merge(Geometry::Vertex3f(25, 25, 10));

    EXPECT_TRUE(BP::BuildPlatePreparator::bboxOverlapXY(a, b));
}

TEST(BuildPlatePreparatorTest, BBoxOverlapXY_WithGap) {
    Geometry::BBox3f a, b;
    a.merge(Geometry::Vertex3f(0, 0, 0));
    a.merge(Geometry::Vertex3f(10, 10, 10));
    b.merge(Geometry::Vertex3f(12, 0, 0));
    b.merge(Geometry::Vertex3f(22, 10, 10));

    // Without gap: no overlap (gap of 2 between them)
    EXPECT_FALSE(BP::BuildPlatePreparator::bboxOverlapXY(a, b, 0.0f));

    // With gap of 5: overlap (since 10+5 > 12)
    EXPECT_TRUE(BP::BuildPlatePreparator::bboxOverlapXY(a, b, 5.0f));
}

TEST(BuildPlatePreparatorTest, ValidateNoOverlap_Passes) {
    auto procA = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    auto procB = makeBoxProcessor(10.0f, 10.0f, 5.0f);

    // Place B at (20, 0, 0) so no overlap
    procB->mesh_->translate(20.0f, 0.0f, 0.0f);
    procB->bbox_.reset();

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    EXPECT_NO_THROW(prep.validateNoOverlap(procs));
}

TEST(BuildPlatePreparatorTest, ValidateNoOverlap_Fails) {
    auto procA = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    auto procB = makeBoxProcessor(10.0f, 10.0f, 5.0f);

    // Both at origin ? overlap
    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    EXPECT_THROW(prep.validateNoOverlap(procs), std::runtime_error);
}

TEST(BuildPlatePreparatorTest, ArrangeModels_TwoBoxes) {
    auto procA = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    auto procB = makeBoxProcessor(10.0f, 10.0f, 5.0f);

    std::vector<BP::ModelPlacement> placements;
    placements.emplace_back();
    placements.back().modelPath = "A.stl";
    placements.emplace_back();
    placements.back().modelPath = "B.stl";

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    prep.setBedSize(100.0f, 100.0f);
    prep.setMinSpacing(5.0);

    bool ok = prep.arrangeModels(placements, procs, 5.0);
    EXPECT_TRUE(ok);

    // After arrangement, models should not overlap
    EXPECT_NO_THROW(prep.validateNoOverlap(procs));

    // Placements should have been updated
    EXPECT_GT(std::abs(placements[0].x - placements[1].x) +
              std::abs(placements[0].y - placements[1].y), 0.0);
}

TEST(BuildPlatePreparatorTest, ArrangeModels_DoesNotFit) {
    // Create models that are too large for the bed
    auto procA = makeBoxProcessor(200.0f, 200.0f, 5.0f);
    auto procB = makeBoxProcessor(200.0f, 200.0f, 5.0f);

    std::vector<BP::ModelPlacement> placements;
    placements.emplace_back();
    placements.emplace_back();

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    prep.setBedSize(250.0f, 250.0f);  // Only fits one 200mm box

    bool ok = prep.arrangeModels(placements, procs, 5.0);
    EXPECT_FALSE(ok);
}

TEST(BuildPlatePreparatorTest, CombinedBoundingBox) {
    auto procA = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    auto procB = makeBoxProcessor(10.0f, 10.0f, 5.0f);

    procB->mesh_->translate(20.0f, 30.0f, 0.0f);
    procB->bbox_.reset();

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    auto bb = prep.combinedBoundingBox(procs);

    EXPECT_FLOAT_EQ(bb.min.x, 0.0f);
    EXPECT_FLOAT_EQ(bb.min.y, 0.0f);
    EXPECT_FLOAT_EQ(bb.max.x, 30.0f);
    EXPECT_FLOAT_EQ(bb.max.y, 40.0f);
    EXPECT_FLOAT_EQ(bb.max.z, 5.0f);
}

TEST(BuildPlatePreparatorTest, AlignAllToGround) {
    auto proc = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    // Move mesh up so min.z is 3.0
    proc->mesh_->translate(0.0f, 0.0f, 3.0f);
    proc->bbox_.reset();
    EXPECT_NEAR(proc->getBoundingBox().min.z, 3.0f, 0.01f);

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(proc));

    BP::BuildPlatePreparator prep;
    prep.alignAllToGround(procs);

    EXPECT_NEAR(procs[0]->getBoundingBox().min.z, 0.0f, 0.01f);
}

TEST(BuildPlatePreparatorTest, PrepareThrowsOnEmpty) {
    std::vector<BP::ModelPlacement> placements;
    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;

    BP::BuildPlatePreparator prep;
    EXPECT_THROW(prep.prepare(placements, procs), std::runtime_error);
}

TEST(BuildPlatePreparatorTest, PrepareThrowsOnMismatch) {
    std::vector<BP::ModelPlacement> placements;
    placements.emplace_back();

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    // No processors — count mismatch

    BP::BuildPlatePreparator prep;
    EXPECT_THROW(prep.prepare(placements, procs), std::runtime_error);
}

TEST(BuildPlatePreparatorTest, FullPrepareWithAutoArrange) {
    // Two overlapping boxes at origin — should be auto-arranged
    auto procA = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    auto procB = makeBoxProcessor(10.0f, 10.0f, 5.0f);

    std::vector<BP::ModelPlacement> placements;
    placements.emplace_back();
    placements.back().modelPath = "A.stl";
    placements.emplace_back();
    placements.back().modelPath = "B.stl";

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    prep.setBedSize(100.0f, 100.0f);
    prep.setMinSpacing(5.0);

    EXPECT_NO_THROW(prep.prepare(placements, procs));

    // After preparation, models should not overlap
    EXPECT_NO_THROW(prep.validateNoOverlap(procs));

    // Both models should be grounded
    for (const auto& p : procs) {
        auto bb = p->getBoundingBox();
        EXPECT_NEAR(bb.min.z, 0.0f, 0.01f);
    }
}

TEST(BuildPlatePreparatorTest, FullPrepareWithSeparateModels) {
    // Two non-overlapping boxes
    auto procA = makeBoxProcessor(10.0f, 10.0f, 5.0f);
    auto procB = makeBoxProcessor(10.0f, 10.0f, 5.0f);

    std::vector<BP::ModelPlacement> placements;
    {
        BP::ModelPlacement plA;
        plA.modelPath = "A.stl";
        plA.x = 0.0;
        plA.y = 0.0;
        placements.push_back(plA);
    }
    {
        BP::ModelPlacement plB;
        plB.modelPath = "B.stl";
        plB.x = 50.0;
        plB.y = 0.0;
        placements.push_back(plB);
    }

    std::vector<std::unique_ptr<Geometry::MeshProcessor>> procs;
    procs.push_back(std::move(procA));
    procs.push_back(std::move(procB));

    BP::BuildPlatePreparator prep;
    prep.setBedSize(100.0f, 100.0f);
    prep.setMinSpacing(5.0);

    EXPECT_NO_THROW(prep.prepare(placements, procs));

    // Model B should be near x=50
    auto bbB = procs[1]->getBoundingBox();
    EXPECT_NEAR(bbB.min.x, 50.0f, 1.0f);
}

// ==============================================================================
// BuildPlate Integration Tests (with ModelPlacement tracking)
// ==============================================================================

TEST(BuildPlateIntegrationTest, PlacementsTracked) {
    // This test verifies that BuildPlate::addModel properly creates
    // ModelPlacement records.
    //
    // NOTE: We can't call addModel here because it requires an actual
    // file on disk.  Instead we test the placements() accessor
    // is empty for a new plate and grows when objects are (hypothetically) added.
    BuildPlate plate;
    EXPECT_TRUE(plate.placements().empty());
}

TEST(BuildPlateIntegrationTest, ClearObjectsClearsPlacements) {
    BuildPlate plate;
    plate.clearObjects();
    EXPECT_TRUE(plate.placements().empty());
    EXPECT_EQ(plate.objectCount(), 0u);
}

TEST(BuildPlateIntegrationTest, PreparedFlagReset) {
    BuildPlate plate;
    EXPECT_FALSE(plate.isPrepared());
    plate.clearObjects();
    EXPECT_FALSE(plate.isPrepared());
}

} // namespace Testing
} // namespace MarcSLM
