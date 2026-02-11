// ==============================================================================
// MarcSLM - Core Types Unit Tests
// ==============================================================================

#include <MarcSLM/Core/Types.hpp>
#include <gtest/gtest.h>

#include <chrono>

using namespace MarcSLM::Core;

// ==============================================================================
// Coordinate Conversion Tests
// ==============================================================================

TEST(CoordinateConversionTest, MillimetersToClipperUnits) {
    // Test conversion from mm to Clipper units
    EXPECT_EQ(mmToClipperUnits(0.0), 0);
    EXPECT_EQ(mmToClipperUnits(1.0), 1000000);
    EXPECT_EQ(mmToClipperUnits(10.5), 10500000);
    EXPECT_EQ(mmToClipperUnits(-5.0), -5000000);
}

TEST(CoordinateConversionTest, ClipperUnitsToMillimeters) {
    // Test conversion from Clipper units to mm
    EXPECT_DOUBLE_EQ(clipperUnitsToMm(0), 0.0);
    EXPECT_DOUBLE_EQ(clipperUnitsToMm(1000000), 1.0);
    EXPECT_DOUBLE_EQ(clipperUnitsToMm(10500000), 10.5);
    EXPECT_DOUBLE_EQ(clipperUnitsToMm(-5000000), -5.0);
}

TEST(CoordinateConversionTest, RoundTripConversion) {
    // Test that conversions are reversible
    std::vector<double> testValues = {0.0, 1.0, -1.0, 123.456, -789.012};
    
    for (double value : testValues) {
        int64_t clipperVal = mmToClipperUnits(value);
        double backToMm = clipperUnitsToMm(clipperVal);
        EXPECT_NEAR(backToMm, value, 1e-6) << "Round-trip failed for " << value;
    }
}

// ==============================================================================
// Point Conversion Tests
// ==============================================================================

TEST(Point2DConversionTest, ToClipperPoint) {
    Point2D pt(5.5, 10.25);
    Point2DInt clipperPt = toClipperPoint(pt);
    
    EXPECT_EQ(clipperPt.x, 5500000);
    EXPECT_EQ(clipperPt.y, 10250000);
}

TEST(Point2DConversionTest, FromClipperPoint) {
    Point2DInt clipperPt(5500000, 10250000);
    Point2D pt = fromClipperPoint(clipperPt);
    
    EXPECT_DOUBLE_EQ(pt.x, 5.5);
    EXPECT_DOUBLE_EQ(pt.y, 10.25);
}

TEST(Point2DConversionTest, RoundTripConversion) {
    Point2D original(12.345, -67.890);
    Point2DInt clipperPt = toClipperPoint(original);
    Point2D converted = fromClipperPoint(clipperPt);
    
    EXPECT_NEAR(converted.x, original.x, 1e-6);
    EXPECT_NEAR(converted.y, original.y, 1e-6);
}

// ==============================================================================
// Slice Structure Tests (MarcSLM::Core::Slice — the pipeline slice)
// ==============================================================================

TEST(CoreSliceTest, DefaultConstruction) {
    Slice slice;
    
    EXPECT_DOUBLE_EQ(slice.zHeight, 0.0);
    EXPECT_EQ(slice.layerIndex, 0u);
    EXPECT_EQ(slice.partID, 0u);
    EXPECT_FALSE(slice.isValid());
    EXPECT_FALSE(slice.hasHoles());
}

TEST(CoreSliceTest, ParameterizedConstruction) {
    Slice slice(5.5, 42);
    
    EXPECT_DOUBLE_EQ(slice.zHeight, 5.5);
    EXPECT_EQ(slice.layerIndex, 42u);
    EXPECT_FALSE(slice.isValid()); // Still no geometry
}

TEST(CoreSliceTest, MoveSemantics) {
    Slice slice1(2.0, 10);
    slice1.outerContour = {
        Point2DInt(0, 0),
        Point2DInt(1000000, 0),
        Point2DInt(1000000, 1000000),
        Point2DInt(0, 1000000)
    };
    
    Slice slice2 = std::move(slice1);
    
    EXPECT_DOUBLE_EQ(slice2.zHeight, 2.0);
    EXPECT_EQ(slice2.layerIndex, 10u);
    EXPECT_EQ(slice2.outerContour.size(), 4u);
    EXPECT_TRUE(slice2.isValid());
}

TEST(CoreSliceTest, ValidityCheck) {
    Slice slice;
    
    // Invalid with no points
    EXPECT_FALSE(slice.isValid());
    
    // Invalid with 2 points
    slice.outerContour = {Point2DInt(0, 0), Point2DInt(1000000, 0)};
    EXPECT_FALSE(slice.isValid());
    
    // Valid with 3 points (triangle)
    slice.outerContour.push_back(Point2DInt(0, 1000000));
    EXPECT_TRUE(slice.isValid());
}

TEST(CoreSliceTest, HoleManagement) {
    Slice slice(1.0, 5);
    
    // Initially no holes
    EXPECT_FALSE(slice.hasHoles());
    EXPECT_EQ(slice.holeCount(), 0u);
    
    // Add a hole
    Clipper2Lib::Path64 hole1 = {
        Point2DInt(100000, 100000),
        Point2DInt(200000, 100000),
        Point2DInt(200000, 200000)
    };
    slice.holes.push_back(std::move(hole1));
    
    EXPECT_TRUE(slice.hasHoles());
    EXPECT_EQ(slice.holeCount(), 1u);
    
    // Add another hole
    Clipper2Lib::Path64 hole2 = {
        Point2DInt(300000, 300000),
        Point2DInt(400000, 300000),
        Point2DInt(400000, 400000)
    };
    slice.holes.push_back(std::move(hole2));
    
    EXPECT_EQ(slice.holeCount(), 2u);
}

TEST(CoreSliceTest, VertexCount) {
    Slice slice;
    
    // Outer contour with 4 points
    slice.outerContour = {
        Point2DInt(0, 0),
        Point2DInt(1000000, 0),
        Point2DInt(1000000, 1000000),
        Point2DInt(0, 1000000)
    };
    
    EXPECT_EQ(slice.vertexCount(), 4u);
    
    // Add a hole with 3 points
    Clipper2Lib::Path64 hole = {
        Point2DInt(250000, 250000),
        Point2DInt(750000, 250000),
        Point2DInt(500000, 750000)
    };
    slice.holes.push_back(std::move(hole));
    
    EXPECT_EQ(slice.vertexCount(), 7u); // 4 outer + 3 hole
}

TEST(CoreSliceTest, MemoryReservation) {
    Slice slice;
    
    // Reserve space for outer contour
    slice.reserveOuter(100);
    EXPECT_GE(slice.outerContour.capacity(), 100u);
    
    // Reserve space for holes
    slice.reserveHoles(10);
    EXPECT_GE(slice.holes.capacity(), 10u);
}

// ==============================================================================
// Performance Tests
// ==============================================================================

TEST(TypesPerformanceTest, CoordinateConversionSpeed) {
    const int iterations = 1000000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        volatile int64_t result = mmToClipperUnits(static_cast<double>(i) * 0.001);
        (void)result; // Suppress unused variable warning
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete in reasonable time (< 100ms for 1M conversions)
    EXPECT_LT(duration.count(), 100000);
}
