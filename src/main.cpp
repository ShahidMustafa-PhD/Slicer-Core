// ==============================================================================
// MarcSLM - Command-Line Interface Entry Point
// ==============================================================================
// Copyright (c) 2024 MarcSLM Project
// Licensed under a commercial-friendly license (non-AGPL)
// ==============================================================================
// Usage:
//   MarcSLM_CLI --config config.json model1.stl [model2.stl ...]
//   MarcSLM_CLI -c config.json model1.3mf model2.obj
//
// Options:
//   --config, -c <path>   Path to JSON configuration file (required)
//   --output, -o <dir>    Output directory for SVG layers (default: ./output)
//   --help,   -h          Print usage information
//   --version, -v         Print version information
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
    std::string              configPath;     ///< Path to JSON config file
    std::string              outputDir;      ///< Output directory (default: ./output)
    std::vector<std::string> modelPaths;     ///< Paths to 3D model files
    bool                     showHelp   = false;
    bool                     showVersion = false;
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
        << "  --help,   -h          Show this help message\n"
        << "  --version, -v         Show version information\n"
        << "\n"
        << "Output Files:\n"
        << "  <output>/SvgLayers/          Raw geometry SVG per layer\n"
        << "  <output>/SvgLayers_slm/      Thermal-classified SVG per layer (22 BuildStyle regions)\n"
        << "  <output>/slicefile.marc      Binary .marc export\n"
        << "\n"
        << "Thermal Region Classification (22 BuildStyleID types):\n"
        << "  Volume:    CoreContour, CoreHatch, Shell1Contour, Shell1Hatch,\n"
        << "             Shell2Contour, Shell2Hatch\n"
        << "  UpSkin:    CoreContour, CoreHatch, Shell1Contour, Shell1Hatch\n"
        << "  DownSkin:  CoreContourOverhang, CoreHatchOverhang,\n"
        << "             Shell1ContourOverhang, Shell1HatchOverhang\n"
        << "  Hollow:    HollowShell1Contour, HollowShell1ContourHatch,\n"
        << "             HollowShell1ContourHatchOverhang,\n"
        << "             HollowShell2Contour, HollowShell2ContourHatch,\n"
        << "             HollowShell2ContourHatchOverhang\n"
        << "  Support:   SupportStructure, SupportContour\n"
        << "\n"
        << "Example:\n"
        << "  " << programName << " -c config.json -o output part_A.stl part_B.stl\n"
        << std::endl;
}

static void printVersion() {
    std::cout
        << MarcSLM::Config::PROJECT_NAME << " v"
        << MarcSLM::Config::PROJECT_VERSION << "\n"
        << MarcSLM::Config::PROJECT_DESCRIPTION << "\n"
        << "C++17 | MSVC x64 | Manifold + Clipper2 + Assimp\n"
        << std::endl;
}

static bool parseArgs(int argc, char* argv[], CliArgs& args) {
    args.outputDir = "output";  // default

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

        // Treat unknown flags as errors
        if (arg.size() > 1 && arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            return false;
        }

        // Positional argument ? model file path
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

    // Build per-model output directory
    std::string modelName = fs::path(modelPath).stem().string();
    fs::path modelOutputDir = fs::path(outputDir) / modelName;

    try {
        fs::create_directories(modelOutputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: Cannot create output directory: " << e.what() << "\n";
        return false;
    }

    // Create the SlmPrint engine for this model
    MarcSLM::SlmPrint engine;
    engine.setSlmConfig(config);
    engine.setProgressCallback(progressCallback);

    // Step 1: Load mesh
    std::cout << "\n--- Step 1: Loading mesh ---\n";
    if (!engine.loadMesh(modelPath)) {
        std::cerr << "Error: Failed to load mesh from " << modelPath << "\n";
        return false;
    }
    std::cout << "Mesh loaded successfully.\n";

    // Step 2: Slice
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
    std::cout << "Slicing complete: " << engine.layerCount() << " layers generated.\n";

    if (engine.layerCount() == 0) {
        std::cerr << "Warning: No layers generated for " << modelPath << "\n";
        return true;  // Not a fatal error
    }

    // Step 3: Generate scan paths (perimeters + hatches)
    std::cout << "\n--- Step 3: Generating scan paths ---\n";
    if (!engine.generatePaths()) {
        std::cerr << "Error: Path generation failed.\n";
        return false;
    }
    std::cout << "Scan paths generated successfully.\n";

    // Step 4: Thermal region classification (22 BuildStyleID types)
    std::cout << "\n--- Step 4: Thermal region classification ---\n";
    auto classifiedLayers = engine.classify();
    std::cout << "Classified " << classifiedLayers.size()
              << " layers into thermal regions.\n";

    // Print classification summary
    {
        size_t totalHatchSegments = 0;
        size_t totalPolylineSegments = 0;
        for (const auto& cl : classifiedLayers) {
            totalHatchSegments += cl.segmentHatches.size();
            totalPolylineSegments += cl.segmentPolylines.size();
        }
        std::cout << "  Hatch segments:    " << totalHatchSegments << "\n";
        std::cout << "  Polyline segments: " << totalPolylineSegments << "\n";
    }

    // Step 5: Export .marc binary file
    std::cout << "\n--- Step 5: Exporting .marc binary ---\n";
    fs::path marcPath = modelOutputDir / "slicefile.marc";
    if (!engine.exportMarc(marcPath.string())) {
        std::cerr << "Error: .marc export failed.\n";
        return false;
    }
    std::cout << "Exported: " << marcPath.string() << "\n";

    // Step 6: Export raw geometry SVG layers
    std::cout << "\n--- Step 6: Exporting raw SVG layers ---\n";
    if (!engine.exportSVG(modelOutputDir.string())) {
        std::cerr << "Error: SVG export failed.\n";
        return false;
    }
    std::cout << "SVG layers written to: "
              << (modelOutputDir / "SvgLayers").string() << "\n";

    // Step 7: Export thermal-classified SVG layers (22 BuildStyleID colour coding)
    std::cout << "\n--- Step 7: Exporting thermal-classified SVG layers ---\n";
    if (!engine.exportSegmentSVG(modelOutputDir.string())) {
        std::cerr << "Error: Segment SVG export failed.\n";
        return false;
    }
    std::cout << "Thermal SVG layers written to: "
              << (modelOutputDir / "SvgLayers_slm").string() << "\n";

    std::cout << "\nModel '" << modelName << "' processed successfully.\n";
    return true;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================

int main(int argc, char* argv[]) {
    // ------------------------------------------------------------------
    // Parse command-line arguments
    // ------------------------------------------------------------------
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    if (args.showHelp) {
        printUsage(argv[0]);
        return EXIT_SUCCESS;
    }
    if (args.showVersion) {
        printVersion();
        return EXIT_SUCCESS;
    }

    if (!validateArgs(args)) {
        std::cerr << "\nRun with --help for usage information.\n";
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------------
    // Print banner
    // ------------------------------------------------------------------
    std::cout
        << "================================================================\n"
        << "  " << MarcSLM::Config::PROJECT_NAME
        << " v" << MarcSLM::Config::PROJECT_VERSION << "\n"
        << "  " << MarcSLM::Config::PROJECT_DESCRIPTION << "\n"
        << "================================================================\n"
        << "\n";

    // ------------------------------------------------------------------
    // Start logger
    // ------------------------------------------------------------------
    MarcSLM::Logger::instance().start();

    // ------------------------------------------------------------------
    // Load JSON configuration
    // ------------------------------------------------------------------
    std::cout << "Loading configuration: " << args.configPath << "\n";
    MarcSLM::SlmConfig config;
    if (!MarcSLM::SlmConfigReader::loadFromFile(args.configPath, config)) {
        std::cerr << "Error: Failed to parse configuration file.\n";
        MarcSLM::Logger::instance().stop();
        return EXIT_FAILURE;
    }

    // Print key configuration values
    std::cout << "\nConfiguration Summary:\n";
    std::cout << "  Beam diameter:       " << config.beam_diameter << " mm\n";
    std::cout << "  Layer thickness:     " << config.layer_thickness << " mm\n";
    std::cout << "  First layer:         " << config.first_layer_thickness << " mm\n";
    std::cout << "  Hatch spacing:       " << config.hatch_spacing << " mm\n";
    std::cout << "  Hatch angle:         " << config.hatch_angle << " deg\n";
    std::cout << "  Island size:         " << config.island_width << " x "
              << config.island_height << " mm\n";
    std::cout << "  Perimeters:          " << config.perimeters << "\n";
    std::cout << "  Threads:             " << config.threads << "\n";
    std::cout << "  Adaptive slicing:    " << (config.adaptive_slicing ? "ON" : "OFF") << "\n";
    std::cout << "  Support material:    " << (config.support_material ? "ON" : "OFF") << "\n";
    std::cout << "  Overhangs angle:     " << config.overhangs_angle << " deg\n";
    std::cout << "  Fill density:        " << config.fill_density << "%\n";

    // ------------------------------------------------------------------
    // Create output directory
    // ------------------------------------------------------------------
    try {
        fs::create_directories(args.outputDir);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: Cannot create output directory '"
                  << args.outputDir << "': " << e.what() << "\n";
        MarcSLM::Logger::instance().stop();
        return EXIT_FAILURE;
    }

    std::cout << "\nOutput directory: " << fs::absolute(args.outputDir).string() << "\n";
    std::cout << "Model files:     " << args.modelPaths.size() << "\n";

    // ------------------------------------------------------------------
    // Process each model through the full SLM pipeline
    // ------------------------------------------------------------------
    int successCount = 0;
    int failCount = 0;

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

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    std::cout << "\n================================================================\n";
    std::cout << "  Pipeline Complete\n";
    std::cout << "================================================================\n";
    std::cout << "  Models processed: " << successCount << " / "
              << args.modelPaths.size() << "\n";
    if (failCount > 0) {
        std::cout << "  Models failed:    " << failCount << "\n";
    }
    std::cout << "  Output directory: " << fs::absolute(args.outputDir).string() << "\n";
    std::cout << "================================================================\n";

    // ------------------------------------------------------------------
    // Shutdown
    // ------------------------------------------------------------------
    MarcSLM::Logger::instance().stop();

    return (failCount == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
