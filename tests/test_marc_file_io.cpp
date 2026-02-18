// ==============================================================================
// MarcSLM - MarcFile Binary I/O Unit Tests
// ==============================================================================

#include <MarcSLM/Core/MarcFile.hpp>
#include <MarcSLM/Core/MarcFormat.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <cstdio>

using namespace MarcSLM;

class MarcFileTest : public ::testing::Test {
protected:
    std::string testFilePath = "test_output.marc";

    void TearDown() override {
        std::remove(testFilePath.c_str());
    }
};

TEST_F(MarcFileTest, WriteAndReadEmpty) {
    MarcFile writer;
    writer.initialize();
    ASSERT_TRUE(writer.writeToFile(testFilePath));

    MarcFile reader;
    ASSERT_TRUE(reader.readFromFile(testFilePath));
    EXPECT_EQ(reader.layers.size(), 0u);
    EXPECT_TRUE(reader.header.isValid());
}

TEST_F(MarcFileTest, WriteAndReadSingleLayer) {
    MarcFile writer;
    writer.initialize();

    Marc::Layer layer(1, 0.05f, 0.03f);

    // Add a hatch
    Marc::Hatch hatch;
    hatch.tag.type = Marc::GeometryType::CoreHatch;
    hatch.lines.emplace_back(0.0f, 0.0f, 10.0f, 0.0f);
    hatch.lines.emplace_back(0.0f, 1.0f, 10.0f, 1.0f);
    layer.hatches.push_back(std::move(hatch));

    // Add a polyline
    Marc::Polyline pl;
    pl.tag.type = Marc::GeometryType::Perimeter;
    pl.points = {Marc::Point(0, 0), Marc::Point(10, 0),
                 Marc::Point(10, 10), Marc::Point(0, 10)};
    layer.polylines.push_back(std::move(pl));

    // Add a polygon
    Marc::Polygon pg;
    pg.tag.type = Marc::GeometryType::SupportStructure;
    pg.points = {Marc::Point(1, 1), Marc::Point(5, 1), Marc::Point(3, 5)};
    layer.polygons.push_back(std::move(pg));

    writer.addLayers({layer});
    ASSERT_TRUE(writer.writeToFile(testFilePath));

    // Read back
    MarcFile reader;
    ASSERT_TRUE(reader.readFromFile(testFilePath));
    ASSERT_EQ(reader.layers.size(), 1u);

    const auto& readLayer = reader.layers[0];
    EXPECT_EQ(readLayer.layerNumber, 1u);
    EXPECT_FLOAT_EQ(readLayer.layerHeight, 0.05f);
    EXPECT_FLOAT_EQ(readLayer.layerThickness, 0.03f);

    ASSERT_EQ(readLayer.hatches.size(), 1u);
    EXPECT_EQ(readLayer.hatches[0].lines.size(), 2u);

    ASSERT_EQ(readLayer.polylines.size(), 1u);
    EXPECT_EQ(readLayer.polylines[0].points.size(), 4u);

    ASSERT_EQ(readLayer.polygons.size(), 1u);
    EXPECT_EQ(readLayer.polygons[0].points.size(), 3u);
}

TEST_F(MarcFileTest, WriteAndReadMultipleLayers) {
    MarcFile writer;
    writer.initialize();

    std::vector<Marc::Layer> layers;
    for (uint32_t i = 0; i < 10; ++i) {
        Marc::Layer layer(i, i * 0.03f, 0.03f);
        Marc::Hatch hatch;
        hatch.lines.emplace_back(0.0f, 0.0f, 10.0f, static_cast<float>(i));
        layer.hatches.push_back(std::move(hatch));
        layers.push_back(std::move(layer));
    }

    writer.addLayers(layers);
    ASSERT_TRUE(writer.writeToFile(testFilePath));

    MarcFile reader;
    ASSERT_TRUE(reader.readFromFile(testFilePath));
    ASSERT_EQ(reader.layers.size(), 10u);

    for (uint32_t i = 0; i < 10; ++i) {
        EXPECT_EQ(reader.layers[i].layerNumber, i);
        EXPECT_FLOAT_EQ(reader.layers[i].layerHeight, i * 0.03f);
    }
}

TEST_F(MarcFileTest, InvalidFileReturnsError) {
    MarcFile reader;
    EXPECT_FALSE(reader.readFromFile("nonexistent_file.marc"));
}

TEST_F(MarcFileTest, HeaderValidation) {
    MarcFile writer;
    writer.initialize();
    EXPECT_TRUE(writer.header.isValid());
    EXPECT_EQ(writer.header.versionMajor(), 1u);
}
