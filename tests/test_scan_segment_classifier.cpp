// ==============================================================================
// MarcSLM - Scan Segment Classifier Pipeline Tests
// ==============================================================================
// Tests each decomposed stage of the PySLM-style thermal classification:
//   Stage 1: LayerPolygonExtractor
//   Stage 2: ThermalMaskGenerator
//   Stage 3: ShellDecomposer
//   Stage 4: ContourHatchSplitter
//   Stage 5: RegionClassifier
//   Integration: ScanSegmentClassifier (orchestrator with TBB)
// ==============================================================================

#include <MarcSLM/Thermal/ClipperBoolean.hpp>
#include <MarcSLM/Thermal/LayerPolygonExtractor.hpp>
#include <MarcSLM/Thermal/ThermalMaskGenerator.hpp>
#include <MarcSLM/Thermal/ShellDecomposer.hpp>
#include <MarcSLM/Thermal/ContourHatchSplitter.hpp>
#include <MarcSLM/Thermal/RegionClassifier.hpp>
#include <MarcSLM/Thermal/ScanSegmentClassifier.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace MarcSLM;
using namespace MarcSLM::Thermal;

// ==============================================================================
// Helpers: Create simple test polygons in Clipper2 integer units
// ==============================================================================

namespace {

/// Build an axis-aligned rectangle as a Clipper2 Path64.
/// Coordinates are in Clipper2 integer units (pre-scaled).
Clipper2Lib::Path64 makeRect(int64_t x, int64_t y, int64_t w, int64_t h) {
    return {
        {x,     y},
        {x + w, y},
        {x + w, y + h},
        {x,     y + h}
    };
}

/// Build a single-path Paths64 from a rectangle.
Clipper2Lib::Paths64 makeRectPaths(int64_t x, int64_t y, int64_t w, int64_t h) {
    return { makeRect(x, y, w, h) };
}

/// Check that a Paths64 is non-empty (has at least one path with >= 3 points).
bool hasGeometry(const Clipper2Lib::Paths64& paths) {
    for (const auto& p : paths) {
        if (p.size() >= 3) return true;
    }
    return false;
}

/// Compute total signed area of a Paths64 (absolute value).
double totalArea(const Clipper2Lib::Paths64& paths) {
    double area = 0.0;
    for (const auto& p : paths) {
        area += std::abs(Clipper2Lib::Area(p));
    }
    return area;
}

/// Build a Marc::Layer with a single square polygon (coordinates in mm).
Marc::Layer makeSquareLayer(uint32_t layerNum, float cx, float cy,
                             float halfSize, float height) {
    Marc::Layer layer;
    layer.layerNumber    = layerNum;
    layer.layerHeight    = height;
    layer.layerThickness = 0.03f;

    Marc::Polygon poly;
    poly.points = {
        {cx - halfSize, cy - halfSize},
        {cx + halfSize, cy - halfSize},
        {cx + halfSize, cy + halfSize},
        {cx - halfSize, cy + halfSize}
    };
    layer.polygons.push_back(std::move(poly));
    return layer;
}

} // anonymous namespace

// ==============================================================================
// Stage 1: ClipperBoolean tests
// ==============================================================================

class ClipperBooleanTest : public ::testing::Test {
protected:
    // Two overlapping rectangles:
    //  A: [0,0] to [100,100]
    //  B: [50,50] to [150,150]
    Clipper2Lib::Paths64 rectA = makeRectPaths(0, 0, 100, 100);
    Clipper2Lib::Paths64 rectB = makeRectPaths(50, 50, 100, 100);
};

TEST_F(ClipperBooleanTest, IntersectOverlapping) {
    auto result = ClipperBoolean::intersect(rectA, rectB);
    ASSERT_TRUE(hasGeometry(result));
    // Intersection should be [50,50]->[100,100] = 50x50 = 2500
    double area = totalArea(result);
    EXPECT_NEAR(area, 2500.0, 10.0);
}

TEST_F(ClipperBooleanTest, DifferenceOverlapping) {
    auto result = ClipperBoolean::difference(rectA, rectB);
    ASSERT_TRUE(hasGeometry(result));
    // A minus intersection: 10000 - 2500 = 7500
    double area = totalArea(result);
    EXPECT_NEAR(area, 7500.0, 10.0);
}

TEST_F(ClipperBooleanTest, UnionOverlapping) {
    auto result = ClipperBoolean::unite(rectA, rectB);
    ASSERT_TRUE(hasGeometry(result));
    // Union: 10000 + 10000 - 2500 = 17500
    double area = totalArea(result);
    EXPECT_NEAR(area, 17500.0, 10.0);
}

TEST_F(ClipperBooleanTest, IntersectEmpty) {
    Clipper2Lib::Paths64 empty;
    auto result = ClipperBoolean::intersect(rectA, empty);
    EXPECT_TRUE(result.empty());
}

TEST_F(ClipperBooleanTest, DifferenceEmpty) {
    Clipper2Lib::Paths64 empty;
    auto result = ClipperBoolean::difference(rectA, empty);
    // A minus nothing = A
    EXPECT_NEAR(totalArea(result), 10000.0, 10.0);
}

TEST_F(ClipperBooleanTest, OffsetInward) {
    // Inward offset a 100x100 rect by 10 -> 80x80 = 6400
    auto result = ClipperBoolean::offsetInward(rectA, 10.0);
    ASSERT_TRUE(hasGeometry(result));
    double area = totalArea(result);
    EXPECT_NEAR(area, 6400.0, 200.0);  // Allow tolerance for miter corners
}

TEST_F(ClipperBooleanTest, OffsetOutward) {
    // Outward offset a 100x100 rect by 10 -> 120x120 = 14400
    auto result = ClipperBoolean::offsetOutward(rectA, 10.0);
    ASSERT_TRUE(hasGeometry(result));
    double area = totalArea(result);
    EXPECT_NEAR(area, 14400.0, 200.0);
}

// ==============================================================================
// Stage 1: LayerPolygonExtractor tests
// ==============================================================================

TEST(LayerPolygonExtractorTest, ExtractSquareLayer) {
    LayerPolygonExtractor extractor(1e4);  // 1mm -> 10000 units
    auto layer = makeSquareLayer(1, 60.0f, 60.0f, 5.0f, 0.03f);

    auto paths = extractor.extract(layer);
    ASSERT_TRUE(hasGeometry(paths));

    // 10mm x 10mm square at scale 1e4:
    //   side = 10 * 1e4 = 1e5 units
    //   area = (1e5)^2 = 1e10
    double area = totalArea(paths);
    EXPECT_NEAR(area, 1e10, 1e7);
}

TEST(LayerPolygonExtractorTest, ExtractEmptyLayer) {
    LayerPolygonExtractor extractor(1e4);
    Marc::Layer empty;
    auto paths = extractor.extract(empty);
    EXPECT_TRUE(paths.empty());
}

TEST(LayerPolygonExtractorTest, CoordinateRoundTrip) {
    LayerPolygonExtractor extractor(1e4);
    EXPECT_EQ(extractor.toClip(1.0), 10000);
    EXPECT_FLOAT_EQ(extractor.fromClip(10000), 1.0f);
    EXPECT_FLOAT_EQ(extractor.fromClip(extractor.toClip(3.14159)), 3.1416f);
}

// ==============================================================================
// Stage 2: ThermalMaskGenerator tests
// ==============================================================================

TEST(ThermalMaskGeneratorTest, FirstLayerIsAllOverhang) {
    auto current = makeRectPaths(0, 0, 1000, 1000);
    auto masks = ThermalMaskGenerator::compute(current, nullptr);

    EXPECT_TRUE(masks.volume.empty());
    EXPECT_TRUE(hasGeometry(masks.overhang));
    EXPECT_NEAR(totalArea(masks.overhang), 1e6, 100.0);
}

TEST(ThermalMaskGeneratorTest, IdenticalLayersAllVolume) {
    auto current = makeRectPaths(0, 0, 1000, 1000);
    auto prev    = makeRectPaths(0, 0, 1000, 1000);

    auto masks = ThermalMaskGenerator::compute(current, &prev);

    EXPECT_TRUE(hasGeometry(masks.volume));
    EXPECT_NEAR(totalArea(masks.volume), 1e6, 100.0);
    // Overhang should be empty (identical layers)
    EXPECT_NEAR(totalArea(masks.overhang), 0.0, 100.0);
}

TEST(ThermalMaskGeneratorTest, PartialOverhang) {
    // Current is wider than previous
    auto current = makeRectPaths(0, 0, 2000, 1000);   // 2000x1000
    auto prev    = makeRectPaths(0, 0, 1000, 1000);   // 1000x1000

    auto masks = ThermalMaskGenerator::compute(current, &prev);

    // Volume = intersection = 1000x1000 = 1e6
    EXPECT_NEAR(totalArea(masks.volume), 1e6, 200.0);
    // Overhang = difference = 1000x1000 = 1e6
    EXPECT_NEAR(totalArea(masks.overhang), 1e6, 200.0);
}

// ==============================================================================
// Stage 3: ShellDecomposer tests
// ==============================================================================

TEST(ShellDecomposerTest, DecomposeSquare) {
    // 1000x1000 square at origin, shell offsets of 100 each
    auto square = makeRectPaths(0, 0, 1000, 1000);
    ShellDecomposer decomposer(100.0, 100.0, 3.0);

    auto result = decomposer.decompose(square);

    // All three regions should have geometry
    EXPECT_TRUE(hasGeometry(result.shell1));
    EXPECT_TRUE(hasGeometry(result.shell2));
    EXPECT_TRUE(hasGeometry(result.core));

    double shell1Area = totalArea(result.shell1);
    double shell2Area = totalArea(result.shell2);
    double coreArea   = totalArea(result.core);

    // Each region must be non-zero
    EXPECT_GT(shell1Area, 0.0);
    EXPECT_GT(shell2Area, 0.0);
    EXPECT_GT(coreArea, 0.0);

    // Shell1 (outermost) should be the largest ring
    EXPECT_GT(shell1Area, shell2Area);

    // Core should be the smallest piece (deeply inset)
    EXPECT_LT(coreArea, shell1Area);

    // Core should be approximately 600x600 = 360000
    // (with miter corners the value is exact for rectangles)
    EXPECT_NEAR(coreArea, 360000.0, 50000.0);
}

TEST(ShellDecomposerTest, ThinPartNoCore) {
    // Very thin rectangle: 50 wide, offset by 100 -> shell1 consumes everything
    auto thin = makeRectPaths(0, 0, 50, 1000);
    ShellDecomposer decomposer(100.0, 100.0, 3.0);

    auto result = decomposer.decompose(thin);

    // Shell1 consumes everything (or mostly)
    // Shell2 and Core should be empty
    EXPECT_NEAR(totalArea(result.core), 0.0, 1.0);
}

TEST(ShellDecomposerTest, EmptyInput) {
    Clipper2Lib::Paths64 empty;
    ShellDecomposer decomposer(100.0, 100.0, 3.0);

    auto result = decomposer.decompose(empty);

    EXPECT_TRUE(result.shell1.empty());
    EXPECT_TRUE(result.shell2.empty());
    EXPECT_TRUE(result.core.empty());
}

// ==============================================================================
// Stage 4: ContourHatchSplitter tests
// ==============================================================================

TEST(ContourHatchSplitterTest, SplitSquare) {
    auto square = makeRectPaths(0, 0, 1000, 1000);
    ContourHatchSplitter splitter(50.0, 0.3, 3.0);

    auto result = splitter.split(square);

    EXPECT_TRUE(hasGeometry(result.contour));
    EXPECT_TRUE(hasGeometry(result.hatch));

    // Hatch should be smaller than original
    double hatchArea = totalArea(result.hatch);
    EXPECT_LT(hatchArea, 1e6);
    EXPECT_GT(hatchArea, 5e5);  // 900x900 = 810000 (roughly)
}

TEST(ContourHatchSplitterTest, ContourHatchTransitionStrip) {
    auto square = makeRectPaths(0, 0, 1000, 1000);
    ContourHatchSplitter splitter(50.0, 0.3, 3.0);

    auto result = splitter.split(square);

    // ContourHatch strip should exist and be non-empty
    EXPECT_TRUE(hasGeometry(result.contourHatch));

    // Should be smaller than the contour ring
    EXPECT_LT(totalArea(result.contourHatch), totalArea(result.contour));
}

TEST(ContourHatchSplitterTest, EmptyInput) {
    Clipper2Lib::Paths64 empty;
    ContourHatchSplitter splitter(50.0, 0.3, 3.0);

    auto result = splitter.split(empty);

    EXPECT_TRUE(result.contour.empty());
    EXPECT_TRUE(result.hatch.empty());
    EXPECT_TRUE(result.contourHatch.empty());
}

// ==============================================================================
// Stage 5: RegionClassifier tests
// ==============================================================================

TEST(RegionClassifierTest, SingleRegionBothMasks) {
    auto region  = makeRectPaths(0, 0, 1000, 1000);
    auto volume  = makeRectPaths(0, 0,  500, 1000);  // Left half
    auto overhang = makeRectPaths(500, 0, 500, 1000); // Right half

    std::vector<TaggedRegion> out;
    RegionClassifier::classify(
        region, volume, overhang,
        ThermalSegmentType::CoreContour_Volume,
        ThermalSegmentType::CoreContour_Overhang,
        out);

    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].type, ThermalSegmentType::CoreContour_Volume);
    EXPECT_EQ(out[1].type, ThermalSegmentType::CoreContour_Overhang);

    // Each should be roughly half the total area
    EXPECT_NEAR(totalArea(out[0].paths), 5e5, 1e4);
    EXPECT_NEAR(totalArea(out[1].paths), 5e5, 1e4);
}

TEST(RegionClassifierTest, ClassifyAllProducesMultipleTypes) {
    auto region  = makeRectPaths(0, 0, 1000, 1000);
    auto volume  = makeRectPaths(0, 0, 1000, 1000);

    Clipper2Lib::Paths64 emptyOverhang;

    RegionClassifier::PhysicalRegions phys;
    phys.coreContour = &region;
    phys.coreHatch   = &region;

    auto results = RegionClassifier::classifyAll(phys, volume, emptyOverhang);

    // Should get volume types for core contour and core hatch
    ASSERT_GE(results.size(), 2u);

    bool hasCoreContourVol = false;
    bool hasCoreNormalHatch = false;
    for (const auto& r : results) {
        if (r.type == ThermalSegmentType::CoreContour_Volume) hasCoreContourVol = true;
        if (r.type == ThermalSegmentType::CoreNormalHatch)    hasCoreNormalHatch = true;
    }
    EXPECT_TRUE(hasCoreContourVol);
    EXPECT_TRUE(hasCoreNormalHatch);
}

TEST(RegionClassifierTest, EmptyRegionProducesNothing) {
    Clipper2Lib::Paths64 empty;
    auto volume = makeRectPaths(0, 0, 1000, 1000);

    std::vector<TaggedRegion> out;
    RegionClassifier::classify(
        empty, volume, empty,
        ThermalSegmentType::CoreContour_Volume,
        ThermalSegmentType::CoreContour_Overhang,
        out);

    EXPECT_TRUE(out.empty());
}

// ==============================================================================
// Integration: ScanSegmentClassifier
// ==============================================================================

class ScanSegmentClassifierTest : public ::testing::Test {
protected:
    SlmConfig config;
    SegmentationParams params;

    void SetUp() override {
        config.beam_diameter         = 0.1;
        config.perimeter_hatch_spacing = 0.2;
        config.layer_thickness       = 0.03;
        config.threads               = 1;   // Serial for deterministic tests

        params.shell1Thickness      = 0.2;
        params.shell2Thickness      = 0.2;
        params.contourWidth         = 0.1;
        params.contourHatchFraction = 0.3;
        params.clipperScale         = 1e4;
        params.miterLimit           = 3.0;
        params.enableParallel       = false;
    }
};

TEST_F(ScanSegmentClassifierTest, SingleLayerNoOverhang) {
    ScanSegmentClassifier classifier(config, params);

    auto layer = makeSquareLayer(1, 60.0f, 60.0f, 5.0f, 0.03f);
    auto result = classifier.classifyLayer(layer);

    EXPECT_EQ(result.layerNumber, 1u);

    // Should have classified regions (no previous layer -> all overhang)
    EXPECT_FALSE(result.regions.empty());

    // Should have some polylines (contours) and/or hatches
    bool hasOutput = !result.segmentPolylines.empty() || !result.segmentHatches.empty();
    EXPECT_TRUE(hasOutput);
}

TEST_F(ScanSegmentClassifierTest, TwoLayersWithOverhangDetection) {
    ScanSegmentClassifier classifier(config, params);

    // Layer 0: small square (5mm x 5mm)
    auto layer0 = makeSquareLayer(0, 60.0f, 60.0f, 2.5f, 0.0f);
    // Layer 1: larger square (10mm x 10mm) -> has overhang on the border
    auto layer1 = makeSquareLayer(1, 60.0f, 60.0f, 5.0f, 0.03f);

    std::vector<Marc::Layer> layers = {layer0, layer1};
    auto results = classifier.classifyAll(layers);

    ASSERT_EQ(results.size(), 2u);

    // Layer 0: first layer -> everything is overhang
    bool layer0HasOverhang = false;
    for (const auto& r : results[0].regions) {
        if (r.type == ThermalSegmentType::CoreOverhangHatch ||
            r.type == ThermalSegmentType::CoreContour_Overhang ||
            r.type == ThermalSegmentType::HollowShell1Contour_Overhang ||
            r.type == ThermalSegmentType::HollowShell2Contour_Overhang ||
            r.type == ThermalSegmentType::HollowShell1OverhangHatch ||
            r.type == ThermalSegmentType::HollowShell2OverhangHatch) {
            layer0HasOverhang = true;
            break;
        }
    }
    EXPECT_TRUE(layer0HasOverhang) << "Layer 0 should have overhang types (first layer)";

    // Layer 1: should have BOTH volume and overhang types
    bool layer1HasVolume = false;
    bool layer1HasOverhang = false;
    for (const auto& r : results[1].regions) {
        switch (r.type) {
            case ThermalSegmentType::CoreContour_Volume:
            case ThermalSegmentType::CoreNormalHatch:
            case ThermalSegmentType::HollowShell1Contour_Volume:
            case ThermalSegmentType::HollowShell1NormalHatch:
            case ThermalSegmentType::HollowShell2Contour_Volume:
            case ThermalSegmentType::HollowShell2NormalHatch:
                layer1HasVolume = true;
                break;
            case ThermalSegmentType::CoreContour_Overhang:
            case ThermalSegmentType::CoreOverhangHatch:
            case ThermalSegmentType::HollowShell1Contour_Overhang:
            case ThermalSegmentType::HollowShell1OverhangHatch:
            case ThermalSegmentType::HollowShell2Contour_Overhang:
            case ThermalSegmentType::HollowShell2OverhangHatch:
                layer1HasOverhang = true;
                break;
            default:
                break;
        }
    }
    EXPECT_TRUE(layer1HasVolume)
        << "Layer 1 should have volume types (supported by smaller layer 0)";
    EXPECT_TRUE(layer1HasOverhang)
        << "Layer 1 should have overhang types (wider than layer 0)";
}

TEST_F(ScanSegmentClassifierTest, EmptyLayerStack) {
    ScanSegmentClassifier classifier(config, params);
    std::vector<Marc::Layer> empty;

    auto results = classifier.classifyAll(empty);
    EXPECT_TRUE(results.empty());
}

TEST_F(ScanSegmentClassifierTest, ClassifyAllPreservesLayerMetadata) {
    ScanSegmentClassifier classifier(config, params);

    auto layer0 = makeSquareLayer(0, 60.0f, 60.0f, 3.0f, 0.0f);
    auto layer1 = makeSquareLayer(1, 60.0f, 60.0f, 3.0f, 0.03f);
    auto layer2 = makeSquareLayer(2, 60.0f, 60.0f, 3.0f, 0.06f);

    std::vector<Marc::Layer> layers = {layer0, layer1, layer2};
    auto results = classifier.classifyAll(layers);

    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].layerNumber, 0u);
    EXPECT_EQ(results[1].layerNumber, 1u);
    EXPECT_EQ(results[2].layerNumber, 2u);
    EXPECT_FLOAT_EQ(results[0].layerHeight, 0.0f);
    EXPECT_FLOAT_EQ(results[1].layerHeight, 0.03f);
    EXPECT_FLOAT_EQ(results[2].layerHeight, 0.06f);
}

TEST_F(ScanSegmentClassifierTest, ParallelProducesSameResults) {
    // Run once serial, once parallel, compare region counts
    params.enableParallel = false;
    ScanSegmentClassifier serialClassifier(config, params);

    params.enableParallel = true;
    ScanSegmentClassifier parallelClassifier(config, params);

    auto layer0 = makeSquareLayer(0, 60.0f, 60.0f, 5.0f, 0.0f);
    auto layer1 = makeSquareLayer(1, 60.0f, 60.0f, 5.0f, 0.03f);
    auto layer2 = makeSquareLayer(2, 60.0f, 60.0f, 5.0f, 0.06f);

    std::vector<Marc::Layer> layers = {layer0, layer1, layer2};

    auto serialResult   = serialClassifier.classifyAll(layers);
    auto parallelResult = parallelClassifier.classifyAll(layers);

    ASSERT_EQ(serialResult.size(), parallelResult.size());

    for (size_t i = 0; i < serialResult.size(); ++i) {
        EXPECT_EQ(serialResult[i].regions.size(),
                  parallelResult[i].regions.size())
            << "Layer " << i << " region count mismatch between serial and parallel";
        EXPECT_EQ(serialResult[i].segmentHatches.size(),
                  parallelResult[i].segmentHatches.size())
            << "Layer " << i << " hatch count mismatch between serial and parallel";
        EXPECT_EQ(serialResult[i].segmentPolylines.size(),
                  parallelResult[i].segmentPolylines.size())
            << "Layer " << i << " polyline count mismatch between serial and parallel";
    }
}

TEST_F(ScanSegmentClassifierTest, PassthroughPreservesTags) {
    ScanSegmentClassifier classifier(config, params);

    Marc::Layer layer;
    layer.layerNumber = 42;
    layer.layerHeight = 1.26f;
    layer.layerThickness = 0.03f;

    // Add a pre-tagged hatch
    Marc::Hatch hatch;
    hatch.tag.type = Marc::GeometryType::CoreHatch;
    hatch.tag.buildStyle = Marc::BuildStyleID::CoreHatch_Volume;
    hatch.lines.emplace_back(0.0f, 0.0f, 10.0f, 0.0f);
    layer.hatches.push_back(std::move(hatch));

    auto result = classifier.classifyLayer(layer);

    // The pre-tagged hatch should appear in segmentHatches
    ASSERT_FALSE(result.segmentHatches.empty());
    EXPECT_EQ(result.segmentHatches[0].type, ThermalSegmentType::CoreNormalHatch);
    EXPECT_EQ(result.segmentHatches[0].hatches.size(), 1u);
}
