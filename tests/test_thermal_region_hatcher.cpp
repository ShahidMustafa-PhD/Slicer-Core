// ==============================================================================
// MarcSLM - Thermal Region Hatcher Unit Tests
// ==============================================================================

#include <MarcSLM/Thermal/ThermalRegionHatcher.hpp>
#include <MarcSLM/Thermal/ThermalSegmentTypes.hpp>
#include <MarcSLM/Thermal/ScanSegmentClassifier.hpp>
#include <MarcSLM/Core/SlmConfig.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <algorithm>

using namespace MarcSLM;
using namespace MarcSLM::Thermal;

// ==============================================================================
// Helpers
// ==============================================================================

namespace {

/// Build a rectangular region in Clipper2 integer units.
Clipper2Lib::Paths64 makeRectRegion(int64_t x, int64_t y, int64_t w, int64_t h) {
    return {{ {x, y}, {x + w, y}, {x + w, y + h}, {x, y + h} }};
}

/// Build a large square region in Clipper2 units (scale 1e4, 10mm = 100000).
Clipper2Lib::Paths64 makeLargeSquare() {
    // 10mm x 10mm at scale 1e4 = 100000 x 100000 units
    return makeRectRegion(500000, 500000, 100000, 100000);
}

/// Check that all hatch lines fall within the bounding box (with tolerance).
bool allLinesInBounds(const std::vector<Marc::Line>& lines,
                      float xMin, float yMin, float xMax, float yMax,
                      float tol = 0.5f) {
    for (const auto& l : lines) {
        if (l.a.x < xMin - tol || l.a.x > xMax + tol ||
            l.a.y < yMin - tol || l.a.y > yMax + tol ||
            l.b.x < xMin - tol || l.b.x > xMax + tol ||
            l.b.y < yMin - tol || l.b.y > yMax + tol) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

// ==============================================================================
// Test Fixture
// ==============================================================================

class ThermalRegionHatcherTest : public ::testing::Test {
protected:
    SlmConfig config;

    void SetUp() override {
        config.hatch_spacing  = 0.1;   // 100 um
        config.hatch_angle    = 45.0;
        config.island_width   = 5.0;
        config.island_height  = 5.0;
        config.beam_diameter  = 0.1;
        config.threads        = 1;
    }
};

// ==============================================================================
// Basic Hatcher Tests
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, BasicHatchProducesLines) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    auto region = makeLargeSquare();
    auto lines = hatcher.hatchRegion(
        region, ThermalSegmentType::HollowShell1NormalHatch, 0);

    ASSERT_FALSE(lines.empty())
        << "Basic hatch should produce lines for Shell1NormalHatch";
    EXPECT_GT(lines.size(), 10u)
        << "10mm region at 0.1mm spacing should produce many hatch lines";
}

TEST_F(ThermalRegionHatcherTest, BasicHatchWithinBounds) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    auto region = makeLargeSquare();
    auto lines = hatcher.hatchRegion(
        region, ThermalSegmentType::HollowShell1NormalHatch, 0);

    // Region is at [50mm, 50mm] to [60mm, 60mm]
    EXPECT_TRUE(allLinesInBounds(lines, 50.0f, 50.0f, 60.0f, 60.0f))
        << "All hatch lines should be clipped within the region boundary";
}

// ==============================================================================
// Island Hatcher Tests
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, IslandHatchProducesLines) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    auto region = makeLargeSquare();
    auto lines = hatcher.hatchRegion(
        region, ThermalSegmentType::CoreNormalHatch, 0);

    ASSERT_FALSE(lines.empty())
        << "Island hatch should produce lines for CoreNormalHatch";
    EXPECT_GT(lines.size(), 10u);
}

TEST_F(ThermalRegionHatcherTest, IslandHatchWithinBounds) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    auto region = makeLargeSquare();
    auto lines = hatcher.hatchRegion(
        region, ThermalSegmentType::CoreNormalHatch, 0);

    EXPECT_TRUE(allLinesInBounds(lines, 50.0f, 50.0f, 60.0f, 60.0f))
        << "Island hatch lines should be within the region";
}

// ==============================================================================
// Contour Types Should NOT Produce Hatches
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, ContourTypesProduceNoHatches) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    auto region = makeLargeSquare();

    auto contourTypes = {
        ThermalSegmentType::CoreContour_Volume,
        ThermalSegmentType::CoreContour_Overhang,
        ThermalSegmentType::HollowShell1Contour_Volume,
        ThermalSegmentType::SupportContourVolume,
        ThermalSegmentType::PointSequence
    };

    for (auto type : contourTypes) {
        auto lines = hatcher.hatchRegion(region, type, 0);
        EXPECT_TRUE(lines.empty())
            << "Contour type " << thermalSegmentToString(type)
            << " should produce no hatch lines";
    }
}

// ==============================================================================
// Layer Angle Rotation
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, LayerRotationChangesAngle) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    auto p0 = hatcher.resolveParams(ThermalSegmentType::CoreNormalHatch, 0);
    auto p1 = hatcher.resolveParams(ThermalSegmentType::CoreNormalHatch, 1);
    auto p2 = hatcher.resolveParams(ThermalSegmentType::CoreNormalHatch, 2);

    // Default rotation is 67 degrees per layer
    EXPECT_NEAR(p1.angle - p0.angle, 67.0, 0.1);
    EXPECT_NEAR(p2.angle - p1.angle, 67.0, 0.1);
}

// ==============================================================================
// Empty Region Handling
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, EmptyRegionProducesNothing) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    Clipper2Lib::Paths64 empty;
    auto lines = hatcher.hatchRegion(
        empty, ThermalSegmentType::CoreNormalHatch, 0);

    EXPECT_TRUE(lines.empty());
}

// ==============================================================================
// Parameter Resolution
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, CoreHatchUsesIslandStrategy) {
    ThermalRegionHatcher hatcher(config);
    auto p = hatcher.resolveParams(ThermalSegmentType::CoreNormalHatch, 0);
    EXPECT_EQ(p.strategy, HatchStrategy::Island);
    EXPECT_DOUBLE_EQ(p.spacing, config.hatch_spacing);
}

TEST_F(ThermalRegionHatcherTest, ShellHatchUsesBasicStrategy) {
    ThermalRegionHatcher hatcher(config);
    auto p = hatcher.resolveParams(ThermalSegmentType::HollowShell1NormalHatch, 0);
    EXPECT_EQ(p.strategy, HatchStrategy::Basic);
}

TEST_F(ThermalRegionHatcherTest, SupportUsesCoarserSpacing) {
    ThermalRegionHatcher hatcher(config);
    auto p = hatcher.resolveParams(ThermalSegmentType::SupportHatch, 0);
    EXPECT_GT(p.spacing, config.hatch_spacing)
        << "Support hatch should use coarser spacing than core";
}

TEST_F(ThermalRegionHatcherTest, ContourHatchTransitionUsesFinerSpacing) {
    ThermalRegionHatcher hatcher(config);
    auto p = hatcher.resolveParams(ThermalSegmentType::CoreContourHatch, 0);
    EXPECT_LT(p.spacing, config.hatch_spacing)
        << "ContourHatch transition should use finer spacing";
}

// ==============================================================================
// Integration: Classifier + Hatcher produces visible hatch lines
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, ClassifiedLayerHasActualHatchVectors) {
    // This test verifies the full pipeline produces non-trivial hatches
    SegmentationParams params;
    params.shell1Thickness     = 0.2;
    params.shell2Thickness     = 0.2;
    params.contourWidth        = 0.1;
    params.clipperScale        = 1e4;
    params.enableParallel      = false;
    params.generateHatchVectors = true;

    ScanSegmentClassifier classifier(config, params);

    // Create a 10mm x 10mm square layer
    Marc::Layer layer;
    layer.layerNumber    = 0;
    layer.layerHeight    = 0.0f;
    layer.layerThickness = 0.03f;

    Marc::Polygon poly;
    poly.points = {
        {55.0f, 55.0f}, {65.0f, 55.0f},
        {65.0f, 65.0f}, {55.0f, 65.0f}
    };
    layer.polygons.push_back(std::move(poly));

    auto result = classifier.classifyLayer(layer);

    // Count total hatch lines across all segments
    size_t totalHatchLines = 0;
    for (const auto& seg : result.segmentHatches) {
        totalHatchLines += seg.hatches.size();
    }

    EXPECT_GT(totalHatchLines, 20u)
        << "Classified layer should have many actual hatch scan vectors";

    // Verify we have both contour polylines and hatch lines
    EXPECT_FALSE(result.segmentPolylines.empty())
        << "Should have contour polyline segments";
    EXPECT_FALSE(result.segmentHatches.empty())
        << "Should have hatch line segments";
}

// ==============================================================================
// Hole-Aware Hatching Tests
// ==============================================================================

TEST_F(ThermalRegionHatcherTest, RegionWithHoleHasFewerHatches) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    // 10mm x 10mm square at [50,50]
    auto regionNoHole = makeLargeSquare();

    // Same square but with a 4mm x 4mm hole in the center
    // Clipper2 NonZero/EvenOdd: CW path = hole
    Clipper2Lib::Paths64 regionWithHole = makeLargeSquare();
    // Add a CW (clockwise) hole: 4mm centered in the 10mm square
    // Square center is at [55mm, 55mm], hole from [53,53] to [57,57]
    Clipper2Lib::Path64 hole = {
        {530000, 530000},
        {530000, 570000},
        {570000, 570000},
        {570000, 530000}
    };
    regionWithHole.push_back(hole);

    auto linesNoHole = hatcher.hatchRegion(
        regionNoHole, ThermalSegmentType::HollowShell1NormalHatch, 0);
    auto linesWithHole = hatcher.hatchRegion(
        regionWithHole, ThermalSegmentType::HollowShell1NormalHatch, 0);

    // Both should produce lines
    ASSERT_FALSE(linesNoHole.empty());
    ASSERT_FALSE(linesWithHole.empty());

    // Compute total hatch lengths
    auto totalLen = [](const std::vector<Marc::Line>& lines) {
        double total = 0.0;
        for (const auto& l : lines) {
            float dx = l.b.x - l.a.x;
            float dy = l.b.y - l.a.y;
            total += std::sqrt(dx * dx + dy * dy);
        }
        return total;
    };

    double lenNoHole = totalLen(linesNoHole);
    double lenWithHole = totalLen(linesWithHole);

    EXPECT_LT(lenWithHole, lenNoHole)
        << "Region with hole should produce shorter total hatch length";
    
    // The hole is 4mm x 4mm in a 10mm x 10mm square = 16% of area removed
    // So total length should be roughly 84% of no-hole length
    double ratio = lenWithHole / lenNoHole;
    EXPECT_GT(ratio, 0.5) << "Hole is small, most hatches should remain";
    EXPECT_LT(ratio, 0.95) << "Hole should visibly reduce hatch coverage";
}

TEST_F(ThermalRegionHatcherTest, IslandHatchWithHoleExcludesHoleRegion) {
    ThermalRegionHatcher hatcher(config);
    hatcher.setClipperScale(1e4);

    // 10mm square with a 4mm hole (island mode = CoreNormalHatch)
    Clipper2Lib::Paths64 region = makeLargeSquare();
    Clipper2Lib::Path64 hole = {
        {530000, 530000},
        {530000, 570000},
        {570000, 570000},
        {570000, 530000}
    };
    region.push_back(hole);

    auto lines = hatcher.hatchRegion(
        region, ThermalSegmentType::CoreNormalHatch, 0);

    ASSERT_FALSE(lines.empty());

    // Verify NO hatch line midpoint falls inside the hole [53,57] x [53,57]
    int insideHole = 0;
    for (const auto& l : lines) {
        float mx = (l.a.x + l.b.x) * 0.5f;
        float my = (l.a.y + l.b.y) * 0.5f;
        if (mx > 53.5f && mx < 56.5f && my > 53.5f && my < 56.5f) {
            insideHole++;
        }
    }
    EXPECT_EQ(insideHole, 0)
        << "No hatch line midpoints should fall inside the hole region";
}
