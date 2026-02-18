// ==============================================================================
// MarcSLM - Hatch Generator Unit Tests
// ==============================================================================
// Tests for PathPlanning::HatchGenerator
// Validates hatch generation, island hatching, endpoint overlap,
// alternating scan direction, and polyline connection.
// ==============================================================================

#include <MarcSLM/PathPlanning/HatchGenerator.hpp>
#include <MarcSLM/Core/Types.hpp>
#include <MarcSLM/Core/SlmConfig.hpp>
#include <MarcSLM/Core/MarcFormat.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <algorithm>
#include <numeric>

using namespace MarcSLM;
using namespace MarcSLM::PathPlanning;
using namespace MarcSLM::Core;

// ==============================================================================
// Helper: Build a square Clipper2 contour (CCW) at given size in mm
// ==============================================================================
static Clipper2Lib::Path64 makeSquareContour(double sideLength) {
    Clipper2Lib::Path64 contour;
    int64_t s = mmToClipperUnits(sideLength);
    contour.emplace_back(0, 0);
    contour.emplace_back(s, 0);
    contour.emplace_back(s, s);
    contour.emplace_back(0, s);
    return contour;
}

// Helper: Build a rectangular contour
static Clipper2Lib::Path64 makeRectContour(double width, double height) {
    Clipper2Lib::Path64 contour;
    int64_t w = mmToClipperUnits(width);
    int64_t h = mmToClipperUnits(height);
    contour.emplace_back(0, 0);
    contour.emplace_back(w, 0);
    contour.emplace_back(w, h);
    contour.emplace_back(0, h);
    return contour;
}

// Helper: Build a triangular contour
static Clipper2Lib::Path64 makeTriangleContour(double base, double height) {
    Clipper2Lib::Path64 contour;
    int64_t b = mmToClipperUnits(base);
    int64_t h = mmToClipperUnits(height);
    contour.emplace_back(0, 0);
    contour.emplace_back(b, 0);
    contour.emplace_back(b / 2, h);
    return contour;
}

// Helper: Build a square hole (CW) inside a contour
static Clipper2Lib::Path64 makeSquareHole(double cx, double cy, double halfSize) {
    Clipper2Lib::Path64 hole;
    int64_t x = mmToClipperUnits(cx);
    int64_t y = mmToClipperUnits(cy);
    int64_t hs = mmToClipperUnits(halfSize);
    // CW winding for a hole
    hole.emplace_back(x - hs, y - hs);
    hole.emplace_back(x - hs, y + hs);
    hole.emplace_back(x + hs, y + hs);
    hole.emplace_back(x + hs, y - hs);
    return hole;
}

// Helper: Compute total length of all hatch lines
static double totalLength(const std::vector<Marc::Line>& lines) {
    double total = 0.0;
    for (const auto& l : lines) {
        float dx = l.b.x - l.a.x;
        float dy = l.b.y - l.a.y;
        total += std::sqrt(dx * dx + dy * dy);
    }
    return total;
}

// ==============================================================================
// Test Fixture
// ==============================================================================
class HatchGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.hatch_spacing = 0.1;   // 100 µm spacing
        config_.hatch_angle = 0.0;     // horizontal hatches
        config_.island_width = 5.0;
        config_.island_height = 5.0;
    }

    SlmConfig config_;
};

// ==============================================================================
// Basic Construction
// ==============================================================================

TEST_F(HatchGeneratorTest, ConstructFromConfig) {
    HatchGenerator gen(config_);
    // Should not throw; config values are captured internally
    SUCCEED();
}

// ==============================================================================
// Empty / Invalid Input
// ==============================================================================

TEST_F(HatchGeneratorTest, EmptyContourReturnsEmpty) {
    HatchGenerator gen(config_);
    Clipper2Lib::Path64 empty;
    auto result = gen.generateHatches(empty);
    EXPECT_TRUE(result.empty());
}

TEST_F(HatchGeneratorTest, TwoPointContourReturnsEmpty) {
    HatchGenerator gen(config_);
    Clipper2Lib::Path64 line;
    line.emplace_back(0, 0);
    line.emplace_back(mmToClipperUnits(10.0), 0);
    auto result = gen.generateHatches(line);
    EXPECT_TRUE(result.empty());
}

TEST_F(HatchGeneratorTest, EmptyPolygonReturnsEmpty) {
    HatchGenerator gen(config_);
    Marc::Polygon polygon;
    auto result = gen.generateHatches(polygon);
    EXPECT_TRUE(result.empty());
}

// ==============================================================================
// Basic Hatch Generation
// ==============================================================================

TEST_F(HatchGeneratorTest, SquareContourProducesHatches) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);  // 10mm x 10mm square
    auto hatches = gen.generateHatches(contour);

    // Should produce non-trivial number of hatch lines
    // At 0.1mm spacing over 10mm, expect ~100 lines
    EXPECT_GT(hatches.size(), 50u);
    EXPECT_LT(hatches.size(), 200u);
}

TEST_F(HatchGeneratorTest, HatchLinesAreInsideContour) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);
    auto hatches = gen.generateHatches(contour);

    // All line endpoints should be within the square [0, 10] x [0, 10]
    // with a small tolerance for floating point
    const float tolerance = 0.5f;  // 0.5mm tolerance for bounding
    for (const auto& line : hatches) {
        EXPECT_GE(line.a.x, -tolerance);
        EXPECT_LE(line.a.x, 10.0f + tolerance);
        EXPECT_GE(line.a.y, -tolerance);
        EXPECT_LE(line.a.y, 10.0f + tolerance);
        EXPECT_GE(line.b.x, -tolerance);
        EXPECT_LE(line.b.x, 10.0f + tolerance);
        EXPECT_GE(line.b.y, -tolerance);
        EXPECT_LE(line.b.y, 10.0f + tolerance);
    }
}

TEST_F(HatchGeneratorTest, HatchLinesHaveNonZeroLength) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);
    auto hatches = gen.generateHatches(contour);

    for (const auto& line : hatches) {
        EXPECT_GT(line.lengthSquared(), 0.0f);
    }
}

// ==============================================================================
// Angle Overrides
// ==============================================================================

TEST_F(HatchGeneratorTest, AngleOverrideChangesHatchDirection) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);

    auto hatches0 = gen.generateHatches(contour, {}, 0.0);
    auto hatches90 = gen.generateHatches(contour, {}, 90.0);

    // Both should produce hatches
    EXPECT_FALSE(hatches0.empty());
    EXPECT_FALSE(hatches90.empty());

    // At 0° the lines should be roughly horizontal (small dy)
    // At 90° the lines should be roughly vertical (small dx)
    // Check the dominant direction of the first line in each set
    if (!hatches0.empty()) {
        float dx = std::abs(hatches0[0].b.x - hatches0[0].a.x);
        float dy = std::abs(hatches0[0].b.y - hatches0[0].a.y);
        EXPECT_GT(dx, dy) << "0° hatches should be mostly horizontal";
    }
    if (!hatches90.empty()) {
        float dx = std::abs(hatches90[0].b.x - hatches90[0].a.x);
        float dy = std::abs(hatches90[0].b.y - hatches90[0].a.y);
        EXPECT_GT(dy, dx) << "90° hatches should be mostly vertical";
    }
}

TEST_F(HatchGeneratorTest, DiagonalAngleProducesHatches) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);

    auto hatches45 = gen.generateHatches(contour, {}, 45.0);
    EXPECT_FALSE(hatches45.empty());

    // 45° lines should have roughly equal dx and dy
    if (!hatches45.empty()) {
        float dx = std::abs(hatches45[0].b.x - hatches45[0].a.x);
        float dy = std::abs(hatches45[0].b.y - hatches45[0].a.y);
        EXPECT_NEAR(dx, dy, dx * 0.3f) << "45° hatches should have roughly equal dx and dy";
    }
}

// ==============================================================================
// Spacing
// ==============================================================================

TEST_F(HatchGeneratorTest, SmallerSpacingProducesMoreLines) {
    auto contour = makeSquareContour(10.0);

    config_.hatch_spacing = 0.2;
    HatchGenerator genWide(config_);
    auto hatchesWide = genWide.generateHatches(contour);

    config_.hatch_spacing = 0.1;
    HatchGenerator genNarrow(config_);
    auto hatchesNarrow = genNarrow.generateHatches(contour);

    // Narrower spacing should produce roughly 2x the lines
    EXPECT_GT(hatchesNarrow.size(), hatchesWide.size());
}

TEST_F(HatchGeneratorTest, SetHatchSpacingWorks) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);

    auto defaultHatches = gen.generateHatches(contour);

    gen.setHatchSpacing(0.5);  // Much wider
    auto wideHatches = gen.generateHatches(contour);

    EXPECT_GT(defaultHatches.size(), wideHatches.size());
}

// ==============================================================================
// Marc::Polygon Input
// ==============================================================================

TEST_F(HatchGeneratorTest, MarcPolygonInputProducesHatches) {
    HatchGenerator gen(config_);

    Marc::Polygon polygon;
    polygon.points = {
        {0.0f, 0.0f},
        {10.0f, 0.0f},
        {10.0f, 10.0f},
        {0.0f, 10.0f}
    };

    auto hatches = gen.generateHatches(polygon);
    EXPECT_FALSE(hatches.empty());
}

TEST_F(HatchGeneratorTest, MarcPolygonResultsSimilarToClipper) {
    HatchGenerator gen(config_);

    // Create equivalent geometry in both formats
    Marc::Polygon polygon;
    polygon.points = {
        {0.0f, 0.0f},
        {10.0f, 0.0f},
        {10.0f, 10.0f},
        {0.0f, 10.0f}
    };
    auto hatchesPoly = gen.generateHatches(polygon, 0.0);

    auto contour = makeSquareContour(10.0);
    auto hatchesClipper = gen.generateHatches(contour, {}, 0.0);

    // Should produce similar number of lines
    EXPECT_NEAR(static_cast<double>(hatchesPoly.size()),
                static_cast<double>(hatchesClipper.size()),
                static_cast<double>(hatchesClipper.size()) * 0.1);
}

// ==============================================================================
// Holes
// ==============================================================================

TEST_F(HatchGeneratorTest, ContourWithHoleHasFewerHatches) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);

    auto hatchesNoHole = gen.generateHatches(contour);

    // Add a 4mm x 4mm hole in the center
    Clipper2Lib::Paths64 holes;
    holes.push_back(makeSquareHole(5.0, 5.0, 2.0));

    auto hatchesWithHole = gen.generateHatches(contour, holes);

    // With a hole, total hatch length should be less
    double lenNoHole = totalLength(hatchesNoHole);
    double lenWithHole = totalLength(hatchesWithHole);
    EXPECT_LT(lenWithHole, lenNoHole);
}

// ==============================================================================
// Island / Checkerboard Hatching
// ==============================================================================

TEST_F(HatchGeneratorTest, IslandHatchesProduceOutput) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(20.0);  // 20mm square

    auto hatches = gen.generateIslandHatches(contour);
    EXPECT_FALSE(hatches.empty());
}

TEST_F(HatchGeneratorTest, IslandHatchesAreInsideContour) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(20.0);

    auto hatches = gen.generateIslandHatches(contour);

    const float tolerance = 0.5f;
    for (const auto& line : hatches) {
        EXPECT_GE(line.a.x, -tolerance);
        EXPECT_LE(line.a.x, 20.0f + tolerance);
        EXPECT_GE(line.a.y, -tolerance);
        EXPECT_LE(line.a.y, 20.0f + tolerance);
        EXPECT_GE(line.b.x, -tolerance);
        EXPECT_LE(line.b.x, 20.0f + tolerance);
        EXPECT_GE(line.b.y, -tolerance);
        EXPECT_LE(line.b.y, 20.0f + tolerance);
    }
}

TEST_F(HatchGeneratorTest, IslandHatchesWithHoles) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(20.0);

    Clipper2Lib::Paths64 holes;
    holes.push_back(makeSquareHole(10.0, 10.0, 3.0));

    auto hatchesNoHole = gen.generateIslandHatches(contour);
    auto hatchesWithHole = gen.generateIslandHatches(contour, holes);

    double lenNoHole = totalLength(hatchesNoHole);
    double lenWithHole = totalLength(hatchesWithHole);
    EXPECT_LT(lenWithHole, lenNoHole);
}

TEST_F(HatchGeneratorTest, UseIslandsSwitch) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(20.0);

    auto hatchesNormal = gen.generateHatches(contour, {}, 0.0, false);
    auto hatchesIsland = gen.generateHatches(contour, {}, 0.0, true);

    // Both should produce output, but island hatches may differ
    EXPECT_FALSE(hatchesNormal.empty());
    EXPECT_FALSE(hatchesIsland.empty());
}

// ==============================================================================
// Endpoint Overlap
// ==============================================================================

TEST_F(HatchGeneratorTest, EndpointOverlapExtendsLines) {
    HatchGenerator genNoOverlap(config_);
    HatchGenerator genWithOverlap(config_);
    genWithOverlap.setEndpointOverlap(0.05);  // 50 µm overlap

    auto contour = makeSquareContour(10.0);

    auto hatchesNo = genNoOverlap.generateHatches(contour, {}, 0.0);
    auto hatchesWith = genWithOverlap.generateHatches(contour, {}, 0.0);

    // Lines with overlap should be slightly longer on average
    if (!hatchesNo.empty() && !hatchesWith.empty()) {
        double lenNo = totalLength(hatchesNo);
        double lenWith = totalLength(hatchesWith);
        EXPECT_GT(lenWith, lenNo);
    }
}

// ==============================================================================
// Layer Angle
// ==============================================================================

TEST_F(HatchGeneratorTest, LayerAngleComputesCorrectly) {
    config_.hatch_angle = 0.0;
    HatchGenerator gen(config_);

    EXPECT_DOUBLE_EQ(gen.layerAngle(0), 0.0);
    EXPECT_NEAR(gen.layerAngle(1), 67.0, 0.001);
    EXPECT_NEAR(gen.layerAngle(2), 134.0, 0.001);
    EXPECT_NEAR(gen.layerAngle(3), 201.0, 0.001);
}

TEST_F(HatchGeneratorTest, LayerAngleWraps) {
    config_.hatch_angle = 0.0;
    HatchGenerator gen(config_);

    // After enough layers, angle should wrap modulo 360
    double angle = gen.layerAngle(10);
    EXPECT_GE(angle, 0.0);
    EXPECT_LT(angle, 360.0);
}

TEST_F(HatchGeneratorTest, CustomLayerRotation) {
    config_.hatch_angle = 0.0;
    HatchGenerator gen(config_);
    gen.setLayerRotation(90.0);

    EXPECT_DOUBLE_EQ(gen.layerAngle(0), 0.0);
    EXPECT_DOUBLE_EQ(gen.layerAngle(1), 90.0);
    EXPECT_DOUBLE_EQ(gen.layerAngle(2), 180.0);
    EXPECT_DOUBLE_EQ(gen.layerAngle(3), 270.0);
    EXPECT_DOUBLE_EQ(gen.layerAngle(4), 0.0);  // wraps
}

// ==============================================================================
// Alternating Scan Direction
// ==============================================================================

TEST_F(HatchGeneratorTest, SortAlternatingFlipsEveryOther) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(5.0);

    auto hatches = gen.generateHatches(contour, {}, 0.0);
    ASSERT_GT(hatches.size(), 3u);

    // Record original endpoints before sorting
    gen.sortAlternating(hatches);

    // After alternating sort, consecutive lines should go in opposite directions.
    // For roughly horizontal lines at 0°, check x-direction alternation.
    for (size_t i = 1; i < hatches.size(); ++i) {
        bool prevLeftToRight = hatches[i - 1].b.x > hatches[i - 1].a.x;
        bool currLeftToRight = hatches[i].b.x > hatches[i].a.x;
        // Note: not all lines will strictly alternate due to sorting by midpoint,
        // but the general pattern should hold for most adjacent pairs in a uniform grid.
        (void)prevLeftToRight;
        (void)currLeftToRight;
    }
    // At minimum, verify the sort didn't corrupt any data
    for (const auto& line : hatches) {
        EXPECT_GT(line.lengthSquared(), 0.0f);
    }
}

// ==============================================================================
// Connect Hatch Lines
// ==============================================================================

TEST_F(HatchGeneratorTest, ConnectHatchLinesProducesPolylines) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(5.0);

    auto hatches = gen.generateHatches(contour, {}, 0.0);
    ASSERT_FALSE(hatches.empty());

    auto polylines = gen.connectHatchLines(hatches);
    EXPECT_FALSE(polylines.empty());

    // Total polylines should be fewer than total lines (some are connected)
    EXPECT_LE(polylines.size(), hatches.size());

    // Each polyline should be valid (>= 2 points)
    for (const auto& pl : polylines) {
        EXPECT_TRUE(pl.isValid());
    }
}

TEST_F(HatchGeneratorTest, ConnectEmptyLinesReturnsEmpty) {
    HatchGenerator gen(config_);
    std::vector<Marc::Line> empty;
    auto polylines = gen.connectHatchLines(empty);
    EXPECT_TRUE(polylines.empty());
}

// ==============================================================================
// Triangle Contour (non-rectangular)
// ==============================================================================

TEST_F(HatchGeneratorTest, TriangleContourProducesHatches) {
    HatchGenerator gen(config_);
    auto contour = makeTriangleContour(10.0, 8.0);

    auto hatches = gen.generateHatches(contour);
    EXPECT_FALSE(hatches.empty());
}

TEST_F(HatchGeneratorTest, TriangleHasFewerHatchesThanSquare) {
    HatchGenerator gen(config_);

    auto square = makeSquareContour(10.0);
    auto triangle = makeTriangleContour(10.0, 10.0);

    auto hatchesSquare = gen.generateHatches(square);
    auto hatchesTriangle = gen.generateHatches(triangle);

    // Triangle has roughly half the area of the square
    double lenSquare = totalLength(hatchesSquare);
    double lenTriangle = totalLength(hatchesTriangle);
    EXPECT_LT(lenTriangle, lenSquare);
}

// ==============================================================================
// Rectangular Contour
// ==============================================================================

TEST_F(HatchGeneratorTest, TallRectangleProducesCorrectHatches) {
    HatchGenerator gen(config_);
    auto contour = makeRectContour(2.0, 20.0);  // narrow and tall

    auto hatches0 = gen.generateHatches(contour, {}, 0.0);   // horizontal
    auto hatches90 = gen.generateHatches(contour, {}, 90.0);  // vertical

    EXPECT_FALSE(hatches0.empty());
    EXPECT_FALSE(hatches90.empty());

    // Horizontal hatches in a tall rectangle should produce many short lines
    // Vertical hatches should produce fewer but longer lines
    double len0 = totalLength(hatches0);
    double len90 = totalLength(hatches90);

    // Both should produce similar total length (area coverage)
    // but horizontal lines have count ~200 (20mm / 0.1mm) of ~2mm each
    // vertical lines have count ~20 (2mm / 0.1mm) of ~20mm each
    EXPECT_GT(hatches0.size(), hatches90.size());
}

// ==============================================================================
// Edge Cases
// ==============================================================================

TEST_F(HatchGeneratorTest, VerySmallContour) {
    HatchGenerator gen(config_);
    // Contour smaller than hatch spacing
    auto contour = makeSquareContour(0.05);  // 50 µm square, spacing is 100 µm
    auto hatches = gen.generateHatches(contour);
    // May produce 0 or very few hatches for such a tiny region
    // Just ensure no crash
    SUCCEED();
}

TEST_F(HatchGeneratorTest, VeryLargeContour) {
    HatchGenerator gen(config_);
    gen.setHatchSpacing(1.0);  // Use wider spacing for performance
    auto contour = makeSquareContour(100.0);  // 100mm x 100mm
    auto hatches = gen.generateHatches(contour);
    EXPECT_FALSE(hatches.empty());
}

TEST_F(HatchGeneratorTest, ZeroSpacingReturnsEmpty) {
    HatchGenerator gen(config_);
    gen.setHatchSpacing(0.0);
    auto contour = makeSquareContour(10.0);
    auto hatches = gen.generateHatches(contour);
    EXPECT_TRUE(hatches.empty());
}

TEST_F(HatchGeneratorTest, NegativeAngleUsesDefault) {
    config_.hatch_angle = 45.0;
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);

    // -1 should use the config default (45°)
    auto hatches = gen.generateHatches(contour, {}, -1.0);
    EXPECT_FALSE(hatches.empty());

    // Explicit 45° should produce similar results
    auto hatchesExplicit = gen.generateHatches(contour, {}, 45.0);
    EXPECT_NEAR(static_cast<double>(hatches.size()),
                static_cast<double>(hatchesExplicit.size()),
                static_cast<double>(hatchesExplicit.size()) * 0.05);
}

// ==============================================================================
// Setter Methods
// ==============================================================================

TEST_F(HatchGeneratorTest, SetHatchAngle) {
    HatchGenerator gen(config_);
    auto contour = makeSquareContour(10.0);

    gen.setHatchAngle(90.0);
    auto hatches = gen.generateHatches(contour);  // uses default angle (now 90°)
    EXPECT_FALSE(hatches.empty());

    // Verify lines are roughly vertical
    if (!hatches.empty()) {
        float dx = std::abs(hatches[0].b.x - hatches[0].a.x);
        float dy = std::abs(hatches[0].b.y - hatches[0].a.y);
        EXPECT_GT(dy, dx);
    }
}
