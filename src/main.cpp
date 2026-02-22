// ==============================================================================
// MarcSLM - Command-Line Interface Entry Point
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// Usage:
//   MarcSLM_CLI --config config.json model1.stl [model2.stl ...]
//   MarcSLM_CLI -c config.json model1.3mf model2.obj
//   MarcSLM_CLI --buildplate -c config.json model1.stl model2.stl
//
// Options:
//   --config, -c <path>     Path to JSON configuration file (required)
//   --output, -o <dir>      Output directory for SVG layers (default: ./output)
//   --buildplate, -b        Process all models as a unified build plate
//   --spacing <mm>          Minimum gap between parts in mm (default: 5)
//   --help,   -h            Print usage information
//   --version, -v           Print version information
//
// Note: bedWidth and bedDepth are read from config.json ("bedWidth", "bedDepth").
//       They are NOT accepted as CLI arguments.
//
// Output:
//   <output>/SvgLayers/          Raw geometry SVG per layer
//   <output>/SvgLayers_slm/      Thermal-classified SVG per layer
//   <output>/slicefile.marc      Binary .marc export
// ==============================================================================

#include "MarcSLM/MarcAPI.hpp"
#include "MarcSLM/SlmPrint.hpp"
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/SlmConfigReader.hpp"
#include "MarcSLM/Core/InternalModel.hpp"
#include "MarcSLM/Core/Logger.hpp"
#include "MarcSLM/Core/Config.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==============================================================================
// CLI Argument Parser
// ==============================================================================

struct CliArgs {
    std::string              configPath;
    std::string              outputDir;
    std::vector<std::string> modelPaths;
    bool                     showHelp    = false;
    bool                     showVersion = false;
    bool                     buildPlate  = false;
    float                    spacing     = 5.0f;  ///< Minimum gap between parts [mm]
    // bedWidth and bedDepth are intentionally absent:
    // they come from config.json ("bedWidth" / "bedDepth").
};

static void printUsage(const char* programName) {
    std::cout
        << "================================================================\n"
        << "  MarcSLM - Industrial DMLS/SLM Slicer CLI  v"
        << MarcSLM::Config::PROJECT_VERSION << "\n"
        << "================================================================\n"
        << "\n"
        << "Usage:\n"
        << "  " << programName << " --config <config.json> [options] <model1> [model2 ...]\n"
        << "\n"
        << "Required:\n"
        << "  --config, -c <path>   Path to JSON configuration file\n"
        << "  <model>               One or more 3D model files (STL, 3MF, OBJ, PLY, FBX)\n"
        << "\n"
        << "Options:\n"
        << "  --output, -o <dir>    Output directory (default: ./output)\n"
        << "  --buildplate, -b      Process all models as a unified build plate\n"
        << "  --spacing <mm>        Minimum gap between parts (default: 5 mm)\n"
        << "  --help,   -h          Show this help message\n"
        << "  --version, -v         Show version information\n"
        << "\n"
        << "Build Plate Dimensions:\n"
        << "  bedWidth and bedDepth are read from the JSON config file.\n"
        << "  Add these keys to your config.json:\n"
        << "    \"bedWidth\": 120.0,\n"
        << "    \"bedDepth\": 120.0\n"
        << "\n"
        << "Output Files:\n"
        << "  <output>/SvgLayers/          Raw geometry SVG per layer\n"
        << "  <output>/SvgLayers_slm/      Thermal-classified SVG per layer\n"
        << "  <output>/slicefile.marc      Binary .marc export\n"
        << "\n"
        << "Example (single model):\n"
        << "  " << programName << " -c config.json -o output part_A.stl\n"
        << "\n"
        << "Example (build plate with multiple models):\n"
        << "  " << programName << " -b -c config.json -o output part_A.stl part_B.stl\n"
        << std::endl;
}

static void printVersion() {
    std::cout
        << MarcSLM::Config::PROJECT_NAME << " v"
        << MarcSLM::Config::PROJECT_VERSION << "\n"
        << MarcSLM::Config::PROJECT_DESCRIPTION << "\n"
        << "C++17 | MSVC x64 | TriMesh + Clipper2 + Assimp\n"
        << std::endl;
}

static bool parseArgs(int argc, char* argv[], CliArgs& args) {
    args.outputDir = "output";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            args.showHelp = true;
            return true;
        }
        if (arg == "--version" || arg == "-v") {
            args.showVersion = true;
            return true;
        }
        if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
            args.configPath = argv[++i];
            continue;
        }
        if ((arg == "--output" || arg == "-o") && i + 1 < argc) {
            args.outputDir = argv[++i];
            continue;
        }
        if (arg == "--buildplate" || arg == "-b") {
            args.buildPlate = true;
            continue;
        }
        if (arg == "--spacing" && i + 1 < argc) {
            args.spacing = std::stof(argv[++i]);
            continue;
        }

        // Reject removed bed-size CLI options with a helpful message
        if (arg == "--bed-width" || arg == "--bed-depth") {
            std::cerr << "Error: '" << arg << "' is no longer a CLI option.\n"
                      << "  Set \"bedWidth\" and \"bedDepth\" in your config.json instead.\n";
            return false;
        }

        if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        args.modelPaths.push_back(arg);
    }

    return true;
}

static bool validateArgs(const CliArgs& args) {
    if (args.showHelp || args.showVersion) return true;

    if (args.configPath.empty()) {
        std::cerr << "Error: --config <path> is required.\n";
        return false;
    }
    if (!fs::exists(args.configPath)) {
        std::cerr << "Error: Config file not found: " << args.configPath << "\n";
        return false;
    }
    if (args.modelPaths.empty()) {
        std::cerr << "Error: At least one model file must be specified.\n";
        return false;
    }
    for (const auto& path : args.modelPaths) {
        if (!fs::exists(path)) {
            std::cerr << "Error: Model file not found: " << path << "\n";
            return false;
        }
    }
    if (args.buildPlate && args.modelPaths.size() < 2) {
        std::cerr << "Warning: --buildplate with a single model is valid but "
                     "equivalent to the default single-model mode.\n";
    }
    return true;
}

// ==============================================================================
// Progress Callback
// ==============================================================================

static void progressCallback(const char* message, int percent) {
    std::cout << "[" << percent << "%] " << message << std::endl;
}

// ==============================================================================
// Pipeline: Process a single model through the full SLM slicer pipeline
// ==============================================================================

static bool processModel(const std::string& modelPath,
                         const MarcSLM::SlmConfig& config,
                         const std::string& outputDir,
                         int modelIndex) {
    std::cout << "\n========================================\n";
    std::cout << "Processing model " << (modelIndex + 1)
              << ": " << fs::path(modelPath).filename().string() << "\n";
    std::cout << "========================================\n";

    std::string modelName = fs::path(modelPath).stem().string();
    fs::path modelOutputDir = fs::path(outputDir) / modelName;

    try {
        fs::create_directories(modelOutputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: Cannot create output directory: " << e.what() << "\n";
        return false;
    }

    MarcSLM::SlmPrint engine;
    engine.setSlmConfig(config);
    // Apply bed size from config to the build plate used for SVG sizing
    engine.buildPlate().setBedSize(
        static_cast<float>(config.bedWidth),
        static_cast<float>(config.bedDepth));
    engine.setProgressCallback(progressCallback);

    std::cout << "\n--- Step 1: Loading mesh ---\n";
    if (!engine.loadMesh(modelPath)) {
        std::cerr << "Error: Failed to load mesh from " << modelPath << "\n";
        return false;
    }

    std::cout << "\n--- Step 2: Slicing ---\n";
    if (config.adaptive_slicing) {
        float minH = static_cast<float>(config.layer_thickness * 0.5);
        float maxH = static_cast<float>(config.layer_thickness);
        if (!engine.sliceAdaptive(minH, maxH, 0.05f)) {
            std::cerr << "Error: Adaptive slicing failed.\n";
            return false;
        }
    } else {
        if (!engine.slice()) {
            std::cerr << "Error: Uniform slicing failed.\n";
            return false;
        }
    }
    std::cout << "Slicing complete: " << engine.layerCount() << " layers.\n";

    if (engine.layerCount() == 0) {
        std::cerr << "Warning: No layers generated for " << modelPath << "\n";
        return true;
    }

    std::cout << "\n--- Step 3: Generating scan paths ---\n";
    if (!engine.generatePaths()) {
        std::cerr << "Error: Path generation failed.\n";
        return false;
    }

    std::cout << "\n--- Step 4: Thermal region classification ---\n";
    auto classifiedLayers = engine.classify();
    std::cout << "Classified " << classifiedLayers.size() << " layers.\n";

    std::cout << "\n--- Step 5: Exporting .marc binary ---\n";
    fs::path marcPath = modelOutputDir / "slicefile.marc";
    if (!engine.exportMarc(marcPath.string())) {
        std::cerr << "Error: .marc export failed.\n";
        return false;
    }

    std::cout << "\n--- Step 6: Exporting raw SVG layers ---\n";
    if (!engine.exportSVG(modelOutputDir.string())) {
        std::cerr << "Error: SVG export failed.\n";
        return false;
    }

    std::cout << "\n--- Step 7: Exporting thermal-classified SVG layers ---\n";
    if (!engine.exportSegmentSVG(modelOutputDir.string())) {
        std::cerr << "Error: Segment SVG export failed.\n";
        return false;
    }

    std::cout << "\nModel '" << modelName << "' processed successfully.\n";
    return true;
}

// ==============================================================================
// Pipeline: Process all models as a unified build plate
// ==============================================================================

static bool processBuildPlate(const std::vector<std::string>& modelPaths,
                               const MarcSLM::SlmConfig& config,
                               const std::string& outputDir,
                               float spacing) {
    // Bed dimensions come exclusively from config
    const float bedWidth = static_cast<float>(config.bedWidth);
    const float bedDepth = static_cast<float>(config.bedDepth);

    std::cout << "\n================================================================\n";
    std::cout << "  Build Plate Mode: " << modelPaths.size() << " models\n";
    std::cout << "  Bed size (from config): " << bedWidth << " x " << bedDepth << " mm\n";
    std::cout << "  Minimum spacing: " << spacing << " mm\n";
    std::cout << "================================================================\n\n";

    std::vector<MarcSLM::InternalModel> models;
    models.reserve(modelPaths.size());

    for (std::size_t i = 0; i < modelPaths.size(); ++i) {
        MarcSLM::InternalModel im;
        im.path        = modelPaths[i];
        im.buildconfig = "";
        im.number      = static_cast<int>(i);
        im.xpos = im.ypos = im.zpos = 0.0;
        im.roll = im.pitch = im.yaw = 0.0;
        models.push_back(im);
        std::cout << "  Model " << i << ": "
                  << fs::path(modelPaths[i]).filename().string() << "\n";
    }

    MarcSLM::SlmPrint engine;
    engine.setSlmConfig(config);
    engine.setProgressCallback(progressCallback);

    // Apply bed size from config
    auto& plate = engine.buildPlate();
    plate.setBedSize(bedWidth, bedDepth);

    std::cout << "\n--- Step 1: Loading models onto build plate ---\n";
    std::size_t added = engine.addModelsToBuildPlate(models);
    if (added == 0) {
        std::cerr << "Error: Failed to add any models to the build plate.\n";
        return false;
    }
    std::cout << "Added " << added << " of " << models.size() << " models.\n";

    std::cout << "\n--- Step 2: Preparing build plate ---\n";
    try {
        plate.prepareBuildPlate();
    } catch (const std::exception& e) {
        std::cerr << "Error: Build plate preparation failed: " << e.what() << "\n";
        return false;
    }

    std::cout << "\n  Model Placement Summary:\n";
    for (std::size_t i = 0; i < plate.objectCount(); ++i) {
        const auto* obj = plate.getObject(i);
        if (!obj) continue;
        std::cout << "    Model " << i << ": pos=("
                  << obj->placement.x << ", " << obj->placement.y << ", "
                  << obj->placement.z << ") size=("
                  << obj->sizeX << " x " << obj->sizeY << " x "
                  << obj->sizeZ << " mm)\n";
    }

    std::cout << "\n--- Step 3: Processing build plate ---\n";
    if (!engine.processBuildPlate()) {
        std::cerr << "Error: Build plate processing failed.\n";
        return false;
    }
    std::cout << "Build plate processing complete: "
              << engine.layerCount() << " layers.\n";

    if (engine.layerCount() == 0) {
        std::cerr << "Warning: No layers generated from the build plate.\n";
        return true;
    }

    std::cout << "\n--- Step 4: Generating scan paths ---\n";
    if (!engine.generatePaths()) {
        std::cerr << "Error: Path generation failed.\n";
        return false;
    }

    std::cout << "\n--- Step 5: Thermal region classification ---\n";
    auto classifiedLayers = engine.classify();
    std::cout << "Classified " << classifiedLayers.size() << " layers.\n";

    fs::path buildPlateOutputDir = fs::path(outputDir) / "buildplate";
    try {
        fs::create_directories(buildPlateOutputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: Cannot create output directory: " << e.what() << "\n";
        return false;
    }

    std::cout << "\n--- Step 6: Exporting .marc binary ---\n";
    fs::path marcPath = buildPlateOutputDir / "slicefile.marc";
    if (!engine.exportMarc(marcPath.string())) {
        std::cerr << "Error: .marc export failed.\n";
        return false;
    }

    std::cout << "\n--- Step 7: Exporting raw SVG layers ---\n";
    if (!engine.exportSVG(buildPlateOutputDir.string())) {
        std::cerr << "Error: SVG export failed.\n";
        return false;
    }

    std::cout << "\n--- Step 8: Exporting thermal-classified SVG layers ---\n";
    if (!engine.exportSegmentSVG(buildPlateOutputDir.string())) {
        std::cerr << "Error: Segment SVG export failed.\n";
        return false;
    }

    MarcSLM::MarcFile verifyFile;
    if (verifyFile.readFromFile(marcPath.string())) {
        std::cout << "\nVerified .marc file: " << verifyFile.layers.size() << " layers\n";
    }

    std::cout << "\nBuild plate processed successfully.\n";
    return true;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================

int main(int argc, char* argv[]) {
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    if (args.showHelp)    { printUsage(argv[0]); return EXIT_SUCCESS; }
    if (args.showVersion) { printVersion();      return EXIT_SUCCESS; }
    if (!validateArgs(args)) {
        std::cerr << "\nRun with --help for usage information.\n";
        return EXIT_FAILURE;
    }

    std::cout
        << "================================================================\n"
        << "  " << MarcSLM::Config::PROJECT_NAME
        << " v" << MarcSLM::Config::PROJECT_VERSION << "\n"
        << "  " << MarcSLM::Config::PROJECT_DESCRIPTION << "\n"
        << "================================================================\n\n";

    MarcSLM::Logger::instance().start();

    // ------------------------------------------------------------------
    // Load JSON configuration — bedWidth / bedDepth come from here
    // ------------------------------------------------------------------
    std::cout << "Loading configuration: " << args.configPath << "\n";
    MarcSLM::SlmConfig config;
    if (!MarcSLM::SlmConfigReader::loadFromFile(args.configPath, config)) {
        std::cerr << "Error: Failed to parse configuration file.\n";
        MarcSLM::Logger::instance().stop();
        return EXIT_FAILURE;
    }

    std::cout << "\nConfiguration Summary:\n"
              << "  Beam diameter:       " << config.beam_diameter       << " mm\n"
              << "  Layer thickness:     " << config.layer_thickness      << " mm\n"
              << "  First layer:         " << config.first_layer_thickness<< " mm\n"
              << "  Hatch spacing:       " << config.hatch_spacing        << " mm\n"
              << "  Hatch angle:         " << config.hatch_angle          << " deg\n"
              << "  Island size:         " << config.island_width  << " x "
                                           << config.island_height << " mm\n"
              << "  Perimeters:          " << config.perimeters           << "\n"
              << "  Threads:             " << config.threads              << "\n"
              << "  Adaptive slicing:    " << (config.adaptive_slicing ? "ON" : "OFF") << "\n"
              << "  Support material:    " << (config.support_material   ? "ON" : "OFF") << "\n"
              << "  Fill density:        " << config.fill_density         << "%\n"
              // Task 2: log bed dimensions read from config
              << "  Bed width  (config): " << config.bedWidth             << " mm\n"
              << "  Bed depth  (config): " << config.bedDepth             << " mm\n";

    try {
        fs::create_directories(args.outputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: Cannot create output directory '"
                  << args.outputDir << "': " << e.what() << "\n";
        MarcSLM::Logger::instance().stop();
        return EXIT_FAILURE;
    }

    std::cout << "\nOutput directory: " << fs::absolute(args.outputDir).string() << "\n"
              << "Model files:     " << args.modelPaths.size() << "\n"
              << "Mode:            " << (args.buildPlate ? "Build Plate" : "Individual") << "\n";

    int successCount = 0;
    int failCount    = 0;

    if (args.buildPlate) {
        // Build plate mode — bed size driven entirely by config
        if (processBuildPlate(args.modelPaths, config, args.outputDir, args.spacing)) {
            successCount = static_cast<int>(args.modelPaths.size());
        } else {
            failCount    = static_cast<int>(args.modelPaths.size());
        }
    } else {
        for (size_t i = 0; i < args.modelPaths.size(); ++i) {
            if (processModel(args.modelPaths[i], config, args.outputDir,
                             static_cast<int>(i))) {
                ++successCount;
            } else {
                ++failCount;
                std::cerr << "Warning: Model " << args.modelPaths[i]
                          << " failed processing.\n";
            }
        }
    }

    std::cout << "\n================================================================\n"
              << "  Pipeline Complete\n"
              << "================================================================\n"
              << "  Mode:             " << (args.buildPlate ? "Build Plate" : "Individual") << "\n"
              << "  Models processed: " << successCount << " / "
                                        << args.modelPaths.size()  << "\n";
    if (failCount > 0)
        std::cout << "  Models failed:    " << failCount << "\n";
    std::cout << "  Output directory: " << fs::absolute(args.outputDir).string() << "\n"
              << "================================================================\n";

    MarcSLM::Logger::instance().stop();
    return (failCount == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
