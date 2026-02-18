// ==============================================================================
// MarcSLM - SlmConfig Unit Tests
// ==============================================================================

#include <MarcSLM/Core/SlmConfig.hpp>
#include <MarcSLM/Core/SlmConfigReader.hpp>
#include <gtest/gtest.h>

using namespace MarcSLM;

TEST(SlmConfigTest, DefaultValues) {
    SlmConfig config;

    EXPECT_DOUBLE_EQ(config.beam_diameter, 0.1);
    EXPECT_DOUBLE_EQ(config.layer_thickness, 0.03);
    EXPECT_DOUBLE_EQ(config.first_layer_thickness, 0.05);
    EXPECT_DOUBLE_EQ(config.hatch_spacing, 0.1);
    EXPECT_DOUBLE_EQ(config.hatch_angle, 45.0);
    EXPECT_DOUBLE_EQ(config.island_width, 5.0);
    EXPECT_DOUBLE_EQ(config.island_height, 5.0);
    EXPECT_EQ(config.perimeters, 3);
    EXPECT_EQ(config.threads, 12);
    EXPECT_DOUBLE_EQ(config.overhangs_angle, 45.0);
    EXPECT_TRUE(config.overhangs);
    EXPECT_TRUE(config.support_material);
    EXPECT_EQ(config.fill_density, 100);
    EXPECT_TRUE(config.fill_gaps);
    EXPECT_TRUE(config.thin_walls);
    EXPECT_TRUE(config.external_perimeters_first);
}

TEST(SlmConfigReaderTest, LoadFromString) {
    SlmConfig config;

    std::string json = R"({
        "beam_diameter": 0.08,
        "layer_thickness": 0.05,
        "hatch_spacing": 0.12,
        "hatch_angle": 67.0,
        "perimeters": 5,
        "threads": 8,
        "overhangs": false,
        "fill_density": 80
    })";

    EXPECT_TRUE(SlmConfigReader::loadFromString(json, config));
    EXPECT_DOUBLE_EQ(config.beam_diameter, 0.08);
    EXPECT_DOUBLE_EQ(config.layer_thickness, 0.05);
    EXPECT_DOUBLE_EQ(config.hatch_spacing, 0.12);
    EXPECT_DOUBLE_EQ(config.hatch_angle, 67.0);
    EXPECT_EQ(config.perimeters, 5);
    EXPECT_EQ(config.threads, 8);
    EXPECT_FALSE(config.overhangs);
    EXPECT_EQ(config.fill_density, 80);
}

TEST(SlmConfigReaderTest, PartialJsonKeepsDefaults) {
    SlmConfig config;

    std::string json = R"({ "beam_diameter": 0.2 })";

    EXPECT_TRUE(SlmConfigReader::loadFromString(json, config));
    EXPECT_DOUBLE_EQ(config.beam_diameter, 0.2);
    // All other values should remain as defaults
    EXPECT_DOUBLE_EQ(config.layer_thickness, 0.03);
    EXPECT_DOUBLE_EQ(config.hatch_spacing, 0.1);
    EXPECT_EQ(config.perimeters, 3);
}

TEST(SlmConfigReaderTest, EmptyJsonKeepsDefaults) {
    SlmConfig config;
    EXPECT_TRUE(SlmConfigReader::loadFromString("{}", config));
    EXPECT_DOUBLE_EQ(config.beam_diameter, 0.1);
}
