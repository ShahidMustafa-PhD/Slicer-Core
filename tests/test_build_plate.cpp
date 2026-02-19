// ==============================================================================
// MarcSLM - BuildPlate Unit Tests
// ==============================================================================
// Tests for build plate preparation pipeline ported from Legacy Print/PrintObject
// ==============================================================================

#include <gtest/gtest.h>

#include "MarcSLM/Core/BuildPlate.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/InternalModel.hpp"

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

} // namespace Testing
} // namespace MarcSLM
