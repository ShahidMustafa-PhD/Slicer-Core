// ==============================================================================
// MarcSLM - Build Style Manager Unit Tests
// ==============================================================================

#include <MarcSLM/Core/BuildStyleManager.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace MarcSLM;

class BuildStyleManagerTest : public ::testing::Test {
protected:
    std::string testFilePath = "test_build_styles.json";

    void SetUp() override {
        // Create a test JSON file
        std::ofstream file(testFilePath);
        file << R"({
  "buildStyles": [
    {
      "id": 1,
      "name": "CoreContour_Volume",
      "description": "core contour on volume",
      "laserId": 1,
      "laserMode": 0,
      "laserPower": 180.0,
      "laserFocus": 0.1,
      "laserSpeed": 600.0,
      "hatchSpacing": 0.09,
      "layerThickness": 0.03,
      "pointDistance": 0.05,
      "pointDelay": 2,
      "pointExposureTime": 100,
      "jumpSpeed": 1500.0,
      "jumpDelay": 1.0
    },
    {
      "id": 8,
      "name": "CoreNormalHatch",
      "description": "core hatch on volume",
      "laserId": 1,
      "laserMode": 0,
      "laserPower": 200.0,
      "laserFocus": 0.15,
      "laserSpeed": 1000.0,
      "hatchSpacing": 0.11,
      "layerThickness": 0.03,
      "pointDistance": 0.07,
      "pointDelay": 1,
      "pointExposureTime": 80,
      "jumpSpeed": 1200.0,
      "jumpDelay": 0.8
    }
  ]
})";
        file.close();
    }

    void TearDown() override {
        std::remove(testFilePath.c_str());
    }
};

TEST_F(BuildStyleManagerTest, LoadFromFile) {
    BuildStyleManager mgr;
    ASSERT_TRUE(mgr.loadFromFile(testFilePath));
    EXPECT_EQ(mgr.count(), 2u);
    EXPECT_FALSE(mgr.empty());
}

TEST_F(BuildStyleManagerTest, FindById) {
    BuildStyleManager mgr;
    ASSERT_TRUE(mgr.loadFromFile(testFilePath));

    const auto* style1 = mgr.findById(1);
    ASSERT_NE(style1, nullptr);
    EXPECT_EQ(style1->name, "CoreContour_Volume");
    EXPECT_DOUBLE_EQ(style1->laserPower, 180.0);

    const auto* style8 = mgr.findById(8);
    ASSERT_NE(style8, nullptr);
    EXPECT_EQ(style8->name, "CoreNormalHatch");
    EXPECT_DOUBLE_EQ(style8->laserSpeed, 1000.0);

    // Non-existent
    EXPECT_EQ(mgr.findById(99), nullptr);
}

TEST_F(BuildStyleManagerTest, SaveAndReload) {
    BuildStyleManager mgr;
    ASSERT_TRUE(mgr.loadFromFile(testFilePath));

    std::string outPath = "test_styles_out.json";
    ASSERT_TRUE(mgr.saveToFile(outPath));

    BuildStyleManager mgr2;
    ASSERT_TRUE(mgr2.loadFromFile(outPath));
    EXPECT_EQ(mgr2.count(), 2u);

    std::remove(outPath.c_str());
}
