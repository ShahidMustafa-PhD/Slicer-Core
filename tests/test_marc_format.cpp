// ==============================================================================
// MarcSLM - MarcFormat Unit Tests
// ==============================================================================
// Comprehensive tests for binary format structures and geometry primitives
// defined in MarcFormat.hpp.
// ==============================================================================

#include <MarcSLM/Core/MarcFormat.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <cstring>

using namespace Marc;

// ==============================================================================
// GeometryCategory
// ==============================================================================

TEST(GeometryCategoryTest, EnumValues) {
    EXPECT_EQ(static_cast<uint32_t>(GeometryCategory::Hatch),    1u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryCategory::Polyline), 2u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryCategory::Polygon),  3u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryCategory::Point),    4u);
}

TEST(GeometryCategoryTest, UnderlyingTypeSize) {
    EXPECT_EQ(sizeof(GeometryCategory), sizeof(uint32_t));
}

TEST(GeometryCategoryTest, TypeAsUint) {
    EXPECT_EQ(typeAsUint(GeometryCategory::Hatch),    1u);
    EXPECT_EQ(typeAsUint(GeometryCategory::Point),     4u);
}

// ==============================================================================
// GeometryType
// ==============================================================================

TEST(GeometryTypeTest, EnumValues) {
    EXPECT_EQ(static_cast<uint32_t>(GeometryType::Undefined),        0u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryType::CoreHatch),        1u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryType::OverhangHatch),    2u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryType::Perimeter),        3u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryType::SupportStructure), 4u);
    EXPECT_EQ(static_cast<uint32_t>(GeometryType::InfillPattern),    5u);
}

TEST(GeometryTypeTest, TypeAsUint) {
    EXPECT_EQ(typeAsUint(GeometryType::Undefined),     0u);
    EXPECT_EQ(typeAsUint(GeometryType::InfillPattern), 5u);
}

// ==============================================================================
// BuildStyleID — all 22 values
// ==============================================================================

TEST(BuildStyleIDTest, VolumeStyles) {
    EXPECT_EQ(typeAsUint(BuildStyleID::CoreContour_Volume),   1u);
    EXPECT_EQ(typeAsUint(BuildStyleID::CoreHatch_Volume),     2u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell1Contour_Volume), 3u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell1Hatch_Volume),   4u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell2Contour_Volume), 5u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell2Hatch_Volume),   6u);
}

TEST(BuildStyleIDTest, UpSkinStyles) {
    EXPECT_EQ(typeAsUint(BuildStyleID::CoreContour_UpSkin),   7u);
    EXPECT_EQ(typeAsUint(BuildStyleID::CoreHatch_UpSkin),     8u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell1Contour_UpSkin), 9u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell1Hatch_UpSkin),   10u);
}

TEST(BuildStyleIDTest, DownSkinStyles) {
    EXPECT_EQ(typeAsUint(BuildStyleID::CoreContourOverhang_DownSkin),   11u);
    EXPECT_EQ(typeAsUint(BuildStyleID::CoreHatchOverhang_DownSkin),     12u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell1ContourOverhang_DownSkin), 13u);
    EXPECT_EQ(typeAsUint(BuildStyleID::Shell1HatchOverhang_DownSkin),   14u);
}

TEST(BuildStyleIDTest, HollowShellStyles) {
    EXPECT_EQ(typeAsUint(BuildStyleID::HollowShell1Contour),              15u);
    EXPECT_EQ(typeAsUint(BuildStyleID::HollowShell1ContourHatch),         16u);
    EXPECT_EQ(typeAsUint(BuildStyleID::HollowShell1ContourHatchOverhang), 17u);
    EXPECT_EQ(typeAsUint(BuildStyleID::HollowShell2Contour),              18u);
    EXPECT_EQ(typeAsUint(BuildStyleID::HollowShell2ContourHatch),         19u);
    EXPECT_EQ(typeAsUint(BuildStyleID::HollowShell2ContourHatchOverhang), 20u);
}

TEST(BuildStyleIDTest, SupportStyles) {
    EXPECT_EQ(typeAsUint(BuildStyleID::SupportStructure), 21u);
    EXPECT_EQ(typeAsUint(BuildStyleID::SupportContour),   22u);
}

TEST(BuildStyleIDTest, BuildStyleToString) {
    EXPECT_STREQ(buildStyleToString(BuildStyleID::CoreContour_Volume),   "CoreContour_Volume");
    EXPECT_STREQ(buildStyleToString(BuildStyleID::CoreHatch_UpSkin),     "CoreHatch_UpSkin");
    EXPECT_STREQ(buildStyleToString(BuildStyleID::SupportStructure),     "SupportStructure");
    EXPECT_STREQ(buildStyleToString(BuildStyleID::SupportContour),       "SupportContour");
    EXPECT_STREQ(buildStyleToString(BuildStyleID::HollowShell2ContourHatchOverhang),
                 "HollowShell2ContourHatchOverhang");

    // Out-of-range sentinel
    EXPECT_STREQ(buildStyleToString(static_cast<BuildStyleID>(999)), "Unknown");
}

// ==============================================================================
// MarcHeader — binary layout
// ==============================================================================

TEST(MarcHeaderTest, ExactSize) {
    EXPECT_EQ(sizeof(MarcHeader), 160u);
}

TEST(MarcHeaderTest, FieldOffsets) {
    // Packed layout — verify every field offset matches the spec table.
    EXPECT_EQ(offsetof(MarcHeader, magic),            0u);
    EXPECT_EQ(offsetof(MarcHeader, version),          4u);
    EXPECT_EQ(offsetof(MarcHeader, totalLayers),      8u);
    EXPECT_EQ(offsetof(MarcHeader, indexTableOffset), 12u);
    EXPECT_EQ(offsetof(MarcHeader, timestamp),        20u);
    EXPECT_EQ(offsetof(MarcHeader, printerId),        28u);
    EXPECT_EQ(offsetof(MarcHeader, reserved),         60u);
}

TEST(MarcHeaderTest, DefaultConstruction) {
    MarcHeader header;

    EXPECT_EQ(header.magic[0], 'M');
    EXPECT_EQ(header.magic[1], 'A');
    EXPECT_EQ(header.magic[2], 'R');
    EXPECT_EQ(header.magic[3], 'C');

    EXPECT_EQ(header.versionMajor(), 1u);
    EXPECT_EQ(header.versionMinor(), 0u);

    EXPECT_EQ(header.totalLayers,      0u);
    EXPECT_EQ(header.indexTableOffset, 0u);
    EXPECT_EQ(header.timestamp,        0u);

    EXPECT_TRUE(header.isValid());
}

TEST(MarcHeaderTest, Validity) {
    MarcHeader header;
    EXPECT_TRUE(header.isValid());

    header.magic[0] = 'X';
    EXPECT_FALSE(header.isValid());

    header.magic[0] = 'M';
    EXPECT_TRUE(header.isValid());
}

TEST(MarcHeaderTest, PrinterIdStorage) {
    MarcHeader header;

    std::strcpy(header.printerId, "EOS_M290");
    EXPECT_STREQ(header.printerId, "EOS_M290");

    const char* longId = "SLM500_UNIT02_SERIAL_1234567";
    std::strncpy(header.printerId, longId, 31);
    header.printerId[31] = '\0';
    EXPECT_EQ(std::strlen(header.printerId), std::strlen(longId));
}

TEST(MarcHeaderTest, VersionEncoding) {
    MarcHeader header;
    // Default is v1.0 ? 0x00010000
    EXPECT_EQ(header.version, 0x00010000u);
    EXPECT_EQ(header.versionMajor(), 1u);
    EXPECT_EQ(header.versionMinor(), 0u);

    // Encode v2.3
    header.version = (uint32_t{2} << 16) | uint32_t{3};
    EXPECT_EQ(header.versionMajor(), 2u);
    EXPECT_EQ(header.versionMinor(), 3u);
}

TEST(MarcHeaderTest, ReservedFieldIsZeroed) {
    MarcHeader header;
    for (size_t i = 0; i < sizeof(header.reserved); ++i) {
        EXPECT_EQ(header.reserved[i], 0u) << "reserved[" << i << "] not zero";
    }
}

// ==============================================================================
// Point
// ==============================================================================

TEST(PointTest, DefaultConstruction) {
    constexpr Point p;
    EXPECT_FLOAT_EQ(p.x, 0.0f);
    EXPECT_FLOAT_EQ(p.y, 0.0f);
    EXPECT_TRUE(p.isZero());
}

TEST(PointTest, ParameterizedConstruction) {
    constexpr Point p(10.5f, 20.25f);
    EXPECT_FLOAT_EQ(p.x, 10.5f);
    EXPECT_FLOAT_EQ(p.y, 20.25f);
    EXPECT_FALSE(p.isZero());
}

TEST(PointTest, ConstexprEvaluation) {
    constexpr Point p;
    static_assert(p.x == 0.0f, "constexpr default x failed");
    static_assert(p.y == 0.0f, "constexpr default y failed");
}

TEST(PointTest, SizeIs8Bytes) {
    EXPECT_EQ(sizeof(Point), 8u);
}

// ==============================================================================
// Line
// ==============================================================================

TEST(LineTest, DefaultConstruction) {
    constexpr Line line;
    EXPECT_TRUE(line.a.isZero());
    EXPECT_TRUE(line.b.isZero());
}

TEST(LineTest, PointConstruction) {
    constexpr Point p1(0.0f, 0.0f);
    constexpr Point p2(3.0f, 4.0f);
    constexpr Line line(p1, p2);

    EXPECT_FLOAT_EQ(line.b.x, 3.0f);
    EXPECT_FLOAT_EQ(line.b.y, 4.0f);
}

TEST(LineTest, CoordinateConstruction) {
    constexpr Line line(1.0f, 2.0f, 4.0f, 6.0f);
    EXPECT_FLOAT_EQ(line.a.x, 1.0f);
    EXPECT_FLOAT_EQ(line.b.y, 6.0f);
}

TEST(LineTest, LengthSquared) {
    constexpr Line line(0.0f, 0.0f, 3.0f, 4.0f);
    EXPECT_FLOAT_EQ(line.lengthSquared(), 25.0f);
}

TEST(LineTest, ConstexprLengthSquared) {
    constexpr Line line(1.0f, 2.0f, 4.0f, 6.0f);
    constexpr float lsq = line.lengthSquared();
    static_assert(lsq == 25.0f, "constexpr lengthSquared failed");
}

TEST(LineTest, SizeIs16Bytes) {
    EXPECT_EQ(sizeof(Line), 16u);
}

// ==============================================================================
// GeometryTag
// ==============================================================================

TEST(GeometryTagTest, DefaultConstruction) {
    constexpr GeometryTag tag;
    EXPECT_EQ(tag.type, GeometryType::Undefined);
    EXPECT_EQ(tag.buildStyle, BuildStyleID::CoreContour_Volume);
    EXPECT_FLOAT_EQ(tag.laserPower, 0.0f);
    EXPECT_FLOAT_EQ(tag.scanSpeed,  0.0f);
    EXPECT_EQ(tag.layerNumber, 0u);
}

TEST(GeometryTagTest, ParameterizedConstruction) {
    constexpr GeometryTag tag(GeometryType::Perimeter,
                              BuildStyleID::CoreContour_UpSkin);
    EXPECT_EQ(tag.type,       GeometryType::Perimeter);
    EXPECT_EQ(tag.buildStyle, BuildStyleID::CoreContour_UpSkin);
}

TEST(GeometryTagTest, LaserParameterOverrides) {
    GeometryTag tag;
    tag.laserPower  = 250.0f;
    tag.scanSpeed   = 850.0f;
    tag.layerNumber = 42;

    EXPECT_FLOAT_EQ(tag.laserPower, 250.0f);
    EXPECT_FLOAT_EQ(tag.scanSpeed,  850.0f);
    EXPECT_EQ(tag.layerNumber, 42u);
}

// ==============================================================================
// Hatch
// ==============================================================================

TEST(HatchTest, DefaultIsEmpty) {
    Hatch hatch;
    EXPECT_TRUE(hatch.empty());
    EXPECT_EQ(hatch.size(), 0u);
}

TEST(HatchTest, AddLines) {
    Hatch hatch;
    hatch.reserve(3);

    hatch.lines.emplace_back(0.0f, 0.0f, 10.0f, 0.0f);
    hatch.lines.emplace_back(0.0f, 1.0f, 10.0f, 1.0f);
    hatch.lines.emplace_back(0.0f, 2.0f, 10.0f, 2.0f);

    EXPECT_FALSE(hatch.empty());
    EXPECT_EQ(hatch.size(), 3u);
}

TEST(HatchTest, TagMetadata) {
    Hatch hatch;
    hatch.tag.type       = GeometryType::CoreHatch;
    hatch.tag.buildStyle = BuildStyleID::CoreHatch_Volume;
    hatch.tag.laserPower = 200.0f;

    EXPECT_EQ(hatch.tag.type, GeometryType::CoreHatch);
    EXPECT_FLOAT_EQ(hatch.tag.laserPower, 200.0f);
}

TEST(HatchTest, ReserveCapacity) {
    Hatch hatch;
    hatch.reserve(1000);
    EXPECT_GE(hatch.lines.capacity(), 1000u);
}

// ==============================================================================
// Polyline
// ==============================================================================

TEST(PolylineTest, DefaultIsInvalid) {
    Polyline pl;
    EXPECT_TRUE(pl.empty());
    EXPECT_FALSE(pl.isValid());
}

TEST(PolylineTest, TwoPointsIsValid) {
    Polyline pl;
    pl.points.emplace_back(0.0f, 0.0f);
    pl.points.emplace_back(10.0f, 10.0f);
    EXPECT_TRUE(pl.isValid());
    EXPECT_EQ(pl.size(), 2u);
}

TEST(PolylineTest, OnePointIsInvalid) {
    Polyline pl;
    pl.points.emplace_back(0.0f, 0.0f);
    EXPECT_FALSE(pl.isValid());
}

// ==============================================================================
// Polygon
// ==============================================================================

TEST(PolygonTest, DefaultIsInvalid) {
    Polygon pg;
    EXPECT_TRUE(pg.empty());
    EXPECT_FALSE(pg.isValid());
}

TEST(PolygonTest, TriangleIsValid) {
    Polygon pg;
    pg.points = {Point(0,0), Point(10,0), Point(5, 8.66f)};
    EXPECT_TRUE(pg.isValid());
    EXPECT_EQ(pg.size(), 3u);
}

TEST(PolygonTest, TwoPointsIsInvalid) {
    Polygon pg;
    pg.points = {Point(0,0), Point(10,0)};
    EXPECT_FALSE(pg.isValid());
}

// ==============================================================================
// Circle
// ==============================================================================

TEST(CircleTest, DefaultIsInvalid) {
    constexpr Circle c;
    EXPECT_TRUE(c.center.isZero());
    EXPECT_FLOAT_EQ(c.radius, 0.0f);
    EXPECT_FALSE(c.isValid());
}

TEST(CircleTest, PositiveRadiusIsValid) {
    constexpr Point center(5.0f, 10.0f);
    constexpr Circle c(center, 3.5f);
    EXPECT_TRUE(c.isValid());
    EXPECT_FLOAT_EQ(c.radius, 3.5f);
}

TEST(CircleTest, NegativeRadiusIsInvalid) {
    Circle c;
    c.radius = -1.0f;
    EXPECT_FALSE(c.isValid());
}

// ==============================================================================
// Slice (Marc::Slice — the ExPolygon replacement)
// ==============================================================================

TEST(MarcSliceTest, DefaultIsEmpty) {
    Slice slice;
    EXPECT_FALSE(slice.isValid());
    EXPECT_FALSE(slice.hasHoles());
    EXPECT_EQ(slice.holeCount(),   0u);
    EXPECT_EQ(slice.vertexCount(), 0u);
}

TEST(MarcSliceTest, ContourOnly) {
    Slice slice;
    slice.contour = {{0, 0}, {1000000, 0}, {1000000, 1000000}, {0, 1000000}};

    EXPECT_TRUE(slice.isValid());
    EXPECT_FALSE(slice.hasHoles());
    EXPECT_EQ(slice.vertexCount(), 4u);
}

TEST(MarcSliceTest, WithHoles) {
    Slice slice;
    slice.contour = {{0, 0}, {1000000, 0}, {1000000, 1000000}, {0, 1000000}};

    Clipper2Lib::Path64 hole1 = {{100000, 100000}, {200000, 100000}, {200000, 200000}};
    Clipper2Lib::Path64 hole2 = {{300000, 300000}, {400000, 300000}, {400000, 400000}};
    slice.holes.push_back(std::move(hole1));
    slice.holes.push_back(std::move(hole2));

    EXPECT_TRUE(slice.hasHoles());
    EXPECT_EQ(slice.holeCount(),   2u);
    EXPECT_EQ(slice.vertexCount(), 10u);   // 4 + 3 + 3
}

TEST(MarcSliceTest, MoveSemantics) {
    Slice s1;
    s1.contour = {{0, 0}, {1000000, 0}, {1000000, 1000000}};

    Slice s2 = std::move(s1);
    EXPECT_TRUE(s2.isValid());
    EXPECT_EQ(s2.contour.size(), 3u);
}

TEST(MarcSliceTest, MemoryReservation) {
    Slice slice;
    slice.reserveContour(100);
    slice.reserveHoles(10);

    EXPECT_GE(slice.contour.capacity(), 100u);
    EXPECT_GE(slice.holes.capacity(),   10u);
}

// ==============================================================================
// Layer
// ==============================================================================

TEST(LayerTest, DefaultConstruction) {
    Layer layer;
    EXPECT_EQ(layer.layerNumber, 0u);
    EXPECT_FLOAT_EQ(layer.layerHeight,    0.0f);
    EXPECT_FLOAT_EQ(layer.layerThickness, 0.0f);
    EXPECT_TRUE(layer.isEmpty());
    EXPECT_EQ(layer.geometryCount(), 0u);
}

TEST(LayerTest, ParameterizedConstruction) {
    Layer layer(42, 2.1f, 0.05f);
    EXPECT_EQ(layer.layerNumber, 42u);
    EXPECT_FLOAT_EQ(layer.layerHeight,    2.1f);
    EXPECT_FLOAT_EQ(layer.layerThickness, 0.05f);
}

TEST(LayerTest, AddMixedGeometry) {
    Layer layer(0, 0.0f, 0.03f);

    Hatch hatch;
    hatch.lines.emplace_back(0.0f, 0.0f, 10.0f, 0.0f);
    layer.hatches.push_back(std::move(hatch));

    Polyline pl;
    pl.points = {Point(0,0), Point(10,10)};
    layer.polylines.push_back(std::move(pl));

    Polygon pg;
    pg.points = {Point(0,0), Point(10,0), Point(5,5)};
    layer.polygons.push_back(std::move(pg));

    EXPECT_FALSE(layer.isEmpty());
    EXPECT_EQ(layer.geometryCount(), 3u);
}

TEST(LayerTest, ReserveCapacity) {
    Layer layer;
    layer.reserveHatches(100);
    layer.reservePolylines(50);
    layer.reservePolygons(200);
    layer.reserveCircles(10);

    EXPECT_GE(layer.hatches.capacity(),   100u);
    EXPECT_GE(layer.polylines.capacity(),  50u);
    EXPECT_GE(layer.polygons.capacity(),  200u);
    EXPECT_GE(layer.circles.capacity(),    10u);
}

TEST(LayerTest, EmptyCheckAfterPush) {
    Layer layer;
    EXPECT_TRUE(layer.isEmpty());

    layer.hatches.emplace_back();
    EXPECT_FALSE(layer.isEmpty());
}

// ==============================================================================
// Integration: full layer workflow
// ==============================================================================

TEST(IntegrationTest, CompleteLayerWorkflow) {
    Layer layer(0, 0.0f, 0.03f);

    // Hatch
    Hatch hatch;
    hatch.tag = GeometryTag(GeometryType::CoreHatch,
                             BuildStyleID::CoreHatch_Volume);
    hatch.reserve(3);
    hatch.lines.emplace_back(0.0f, 0.0f, 10.0f, 0.0f);
    hatch.lines.emplace_back(0.0f, 1.0f, 10.0f, 1.0f);
    hatch.lines.emplace_back(0.0f, 2.0f, 10.0f, 2.0f);
    layer.hatches.push_back(std::move(hatch));

    // Perimeter
    Polyline perimeter;
    perimeter.tag = GeometryTag(GeometryType::Perimeter,
                                 BuildStyleID::CoreContour_Volume);
    perimeter.points = {
        Point(0.0f, 0.0f), Point(10.0f, 0.0f),
        Point(10.0f, 10.0f), Point(0.0f, 10.0f)
    };
    layer.polylines.push_back(std::move(perimeter));

    EXPECT_FALSE(layer.isEmpty());
    EXPECT_EQ(layer.geometryCount(), 2u);
    EXPECT_EQ(layer.hatches[0].size(),   3u);
    EXPECT_EQ(layer.polylines[0].size(), 4u);
}

// ==============================================================================
// Micro-benchmarks (sanity, not regression gates)
// ==============================================================================

TEST(MarcPerfTest, PointConstruction1M) {
    const int N = 1000000;
    auto t0 = std::chrono::high_resolution_clock::now();

    volatile float sum = 0.0f;
    for (int i = 0; i < N; ++i) {
        Point p(static_cast<float>(i), static_cast<float>(i * 2));
        sum += p.x + p.y;
    }

    auto dt = std::chrono::high_resolution_clock::now() - t0;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::microseconds>(dt).count(),
              50000);
}

TEST(MarcPerfTest, LineLengthSquared1M) {
    const int N = 1000000;
    auto t0 = std::chrono::high_resolution_clock::now();

    volatile float sum = 0.0f;
    for (int i = 0; i < N; ++i) {
        Line l(0.0f, 0.0f, static_cast<float>(i), static_cast<float>(i));
        sum += l.lengthSquared();
    }

    auto dt = std::chrono::high_resolution_clock::now() - t0;
    EXPECT_LT(std::chrono::duration_cast<std::chrono::microseconds>(dt).count(),
              100000);
}
