// ==============================================================================
// MarcSLM - Thermal Segment Types Unit Tests
// ==============================================================================

#include <MarcSLM/Thermal/ThermalSegmentTypes.hpp>
#include <gtest/gtest.h>

using namespace MarcSLM;

TEST(ThermalSegmentTypeTest, EnumValues) {
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::CoreContour_Volume), 1u);
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::CoreContour_Overhang), 2u);
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::CoreOverhangHatch), 7u);
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::CoreNormalHatch), 8u);
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::SupportHatch), 17u);
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::ExternalSupports), 19u);
    EXPECT_EQ(static_cast<uint32_t>(ThermalSegmentType::HollowShell2ContourHatchOverhang), 22u);
}

TEST(ThermalSegmentTypeTest, ToString) {
    EXPECT_STREQ(thermalSegmentToString(ThermalSegmentType::CoreContour_Volume),
                 "CoreContour_Volume");
    EXPECT_STREQ(thermalSegmentToString(ThermalSegmentType::CoreNormalHatch),
                 "CoreNormalHatch");
    EXPECT_STREQ(thermalSegmentToString(ThermalSegmentType::ExternalSupports),
                 "ExternalSupports");
    EXPECT_STREQ(thermalSegmentToString(static_cast<ThermalSegmentType>(999)),
                 "Unknown");
}

TEST(ThermalSegmentTypeTest, Colors) {
    EXPECT_STREQ(thermalSegmentColor(ThermalSegmentType::CoreContour_Volume), "black");
    EXPECT_STREQ(thermalSegmentColor(ThermalSegmentType::CoreNormalHatch), "green");
    EXPECT_STREQ(thermalSegmentColor(ThermalSegmentType::CoreOverhangHatch), "orange");
    EXPECT_STREQ(thermalSegmentColor(ThermalSegmentType::ExternalSupports), "red");
}

TEST(ScanSegmentHatchTest, DefaultConstruction) {
    ScanSegmentHatch seg;
    EXPECT_TRUE(seg.hatches.empty());
    EXPECT_EQ(seg.type, ThermalSegmentType::CoreNormalHatch);
}

TEST(ScanSegmentPolylineTest, DefaultConstruction) {
    ScanSegmentPolyline seg;
    EXPECT_TRUE(seg.polylines.empty());
    EXPECT_EQ(seg.type, ThermalSegmentType::CoreContour_Volume);
}
