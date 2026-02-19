// ==============================================================================
// MarcSLM - SLM Print Engine Implementation
// ==============================================================================
// Ported from Legacy SlmPrint.cpp + MarcAPI.cpp pipeline logic
// ==============================================================================

#include "MarcSLM/SlmPrint.hpp"
#include "MarcSLM/Core/SlmConfigReader.hpp"
#include "MarcSLM/Core/SVGExporter.hpp"

#include <filesystem>
#include <iostream>
#include <cmath>

namespace MarcSLM {

SlmPrint::SlmPrint() = default;

// =========================================================================
// Configuration
// =========================================================================

bool SlmPrint::loadSlmConfig(const std::filesystem::path& configPath) {
    try {
        if (!std::filesystem::exists(configPath)) {
            std::cerr << "SlmPrint: Config file not found: " << configPath << std::endl;
            return false;
        }
        bool ok = SlmConfigReader::loadFromFile(configPath.string(), config_);
        if (ok) {
            buildPlate_.applyConfig(config_);
        }
        return ok;
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: " << e.what() << std::endl;
        return false;
    }
}

bool SlmPrint::loadBuildStyles(const std::filesystem::path& stylesPath) {
    try {
        if (!std::filesystem::exists(stylesPath)) {
            std::cerr << "SlmPrint: Styles file not found: " << stylesPath << std::endl;
            return false;
        }
        return buildStyles_.loadFromFile(stylesPath);
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: " << e.what() << std::endl;
        return false;
    }
}

void SlmPrint::setSlmConfig(const SlmConfig& config) {
    config_ = config;
    buildPlate_.applyConfig(config_);
}

// =========================================================================
// Model Loading (Single-Mesh Legacy Path)
// =========================================================================

bool SlmPrint::loadMesh(const std::string& filePath) {
    try {
        reportProgress("Loading mesh...", 5);
        meshProcessor_.loadMesh(filePath);
        reportProgress("Mesh loaded successfully", 10);
        return meshProcessor_.hasValidMesh();
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: Failed to load mesh: " << e.what() << std::endl;
        return false;
    }
}

bool SlmPrint::hasMesh() const noexcept {
    return meshProcessor_.hasValidMesh();
}

// =========================================================================
// Build Plate Management (Multi-Model Path)
// =========================================================================

PrintObject* SlmPrint::addModelToBuildPlate(const InternalModel& model) {
    buildPlate_.applyConfig(config_);
    return buildPlate_.addModel(model);
}

size_t SlmPrint::addModelsToBuildPlate(const std::vector<InternalModel>& models) {
    buildPlate_.applyConfig(config_);
    return buildPlate_.addModels(models);
}

bool SlmPrint::processBuildPlate() {
    try {
        reportProgress("Processing build plate...", 10);
        buildPlate_.setProgressCallback(progressCb_);
        buildPlate_.process();

        // Extract layers from build plate
        layers_ = buildPlate_.exportLayers();

        reportProgress("Build plate processing complete", 50);
        std::cout << "SlmPrint: Build plate generated " << layers_.size()
                  << " layers" << std::endl;
        return !layers_.empty();
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: Build plate processing failed: "
                  << e.what() << std::endl;
        return false;
    }
}

std::vector<Marc::Layer> SlmPrint::exportBuildPlateLayers() const {
    return buildPlate_.exportLayers();
}

// =========================================================================
// Slicing (Single-Mesh Legacy Path)
// =========================================================================

bool SlmPrint::slice() {
    return sliceUniform(static_cast<float>(config_.layer_thickness));
}

bool SlmPrint::sliceUniform(float layerThickness) {
    try {
        if (!hasMesh()) {
            std::cerr << "SlmPrint: No mesh loaded for slicing" << std::endl;
            return false;
        }

        reportProgress("Slicing...", 20);
        layers_ = meshProcessor_.sliceUniform(layerThickness);
        reportProgress("Slicing complete", 40);

        std::cout << "SlmPrint: Generated " << layers_.size() << " layers" << std::endl;
        return !layers_.empty();
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: Slicing failed: " << e.what() << std::endl;
        return false;
    }
}

bool SlmPrint::sliceAdaptive(float minHeight, float maxHeight, float maxError) {
    try {
        if (!hasMesh()) {
            std::cerr << "SlmPrint: No mesh loaded for slicing" << std::endl;
            return false;
        }

        reportProgress("Adaptive slicing...", 20);
        layers_ = meshProcessor_.sliceAdaptive(minHeight, maxHeight, maxError);
        reportProgress("Adaptive slicing complete", 40);

        std::cout << "SlmPrint: Generated " << layers_.size()
                  << " adaptive layers" << std::endl;
        return !layers_.empty();
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: Adaptive slicing failed: " << e.what() << std::endl;
        return false;
    }
}

const Marc::Layer* SlmPrint::getLayer(size_t index) const {
    return (index < layers_.size()) ? &layers_[index] : nullptr;
}

// =========================================================================
// Path Generation
// =========================================================================

bool SlmPrint::generatePaths() {
    if (layers_.empty()) {
        std::cerr << "SlmPrint: No layers to generate paths for" << std::endl;
        return false;
    }

    reportProgress("Generating scan paths...", 50);

    PathPlanning::HatchGenerator hatchGen(config_);

    for (size_t i = 0; i < layers_.size(); ++i) {
        auto& layer = layers_[i];

        // For each polyline (contour) in the layer, generate hatches
        for (const auto& polyline : layer.polylines) {
            if (polyline.points.size() < 3) continue;

            // Convert polyline to polygon for hatching
            Marc::Polygon polygon;
            polygon.points = polyline.points;
            polygon.tag = polyline.tag;
            polygon.tag.type = Marc::GeometryType::CoreHatch;
            polygon.tag.buildStyle = Marc::BuildStyleID::CoreHatch_Volume;

            // Rotate hatch angle by 67° per layer (industry standard)
            double layerAngle = config_.hatch_angle + (i * 67.0);
            layerAngle = std::fmod(layerAngle, 360.0);

            auto hatchLines = hatchGen.generateHatches(polygon, layerAngle);

            if (!hatchLines.empty()) {
                Marc::Hatch hatch;
                hatch.tag.type = Marc::GeometryType::CoreHatch;
                hatch.tag.buildStyle = Marc::BuildStyleID::CoreHatch_Volume;
                hatch.tag.layerNumber = layer.layerNumber;
                hatch.lines = std::move(hatchLines);
                layer.hatches.push_back(std::move(hatch));
            }
        }

        // Report progress periodically
        if (i % 100 == 0) {
            int pct = 50 + static_cast<int>(30.0 * i / layers_.size());
            reportProgress("Generating paths...", pct);
        }
    }

    reportProgress("Path generation complete", 80);
    return true;
}

// =========================================================================
// Thermal Classification
// =========================================================================

std::vector<ClassifiedLayer> SlmPrint::classify() const {
    ScanSegmentClassifier classifier(config_);
    return classifier.classifyAll(layers_);
}

// =========================================================================
// Export
// =========================================================================

bool SlmPrint::exportMarc(const std::string& outputPath) {
    try {
        reportProgress("Exporting .marc file...", 85);

        MarcFile marcFile;
        marcFile.initialize();
        marcFile.addLayers(layers_);

        bool success = marcFile.writeToFile(outputPath);

        if (success) {
            reportProgress("Export complete", 90);
            std::cout << "SlmPrint: Exported to " << outputPath << std::endl;
        }
        return success;
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: Export failed: " << e.what() << std::endl;
        return false;
    }
}

bool SlmPrint::exportSVG(const std::string& outputDir) {
    try {
        std::filesystem::path svgDir = std::filesystem::path(outputDir) / "SvgLayers";
        std::filesystem::create_directories(svgDir);

        reportProgress("Exporting SVG layers...", 90);

        for (const auto& layer : layers_) {
            std::string outputPath =
                (svgDir / ("Layer" + std::to_string(layer.layerNumber) + ".svg")).string();

            SVGExporter svg(outputPath.c_str());

            // Draw origin marker
            svg.draw(Marc::Point(0.0f, 0.0f), "red", 0.1f);
            svg.draw(Marc::Point(0.0f, 0.0f), "none", 80.0f);

            // Draw hatches
            for (const auto& hatch : layer.hatches) {
                svg.draw(hatch.lines, "green", 0.4f);
            }

            // Draw polylines (contours/perimeters)
            for (const auto& polyline : layer.polylines) {
                svg.draw(polyline, "black", 0.2f);
            }

            // Draw polygons
            for (const auto& polygon : layer.polygons) {
                svg.draw(polygon, "red", 0.2f);
            }

            svg.Close();
        }

        reportProgress("SVG export complete", 95);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: SVG export failed: " << e.what() << std::endl;
        return false;
    }
}

bool SlmPrint::exportSegmentSVG(const std::string& outputDir) {
    try {
        std::filesystem::path svgDir =
            std::filesystem::path(outputDir) / "SvgLayers_slm";
        std::filesystem::create_directories(svgDir);

        auto classified = classify();

        for (const auto& clayer : classified) {
            std::string outputPath =
                (svgDir / ("Layer" + std::to_string(clayer.layerNumber) + ".svg")).string();

            SVGExporter svg(outputPath.c_str());

            svg.draw(Marc::Point(0.0f, 0.0f), "red", 0.1f);
            svg.draw(Marc::Point(0.0f, 0.0f), "none", 80.0f);

            // Draw hatch segments with thermal colours
            for (const auto& seg : clayer.segmentHatches) {
                std::string color = thermalSegmentColor(seg.type);
                svg.draw(seg.hatches, color, 0.4f);
            }

            // Draw polyline segments with thermal colours
            for (const auto& seg : clayer.segmentPolylines) {
                std::string color = thermalSegmentColor(seg.type);
                svg.draw(seg.polylines, color, 0.2f);
            }

            svg.Close();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "SlmPrint: Segment SVG export failed: " << e.what() << std::endl;
        return false;
    }
}

bool SlmPrint::processAndExport(const std::string& outputDir) {
    reportProgress("Starting full pipeline...", 0);

    // Step 1: Slice
    if (!slice()) {
        std::cerr << "SlmPrint: Slicing failed" << std::endl;
        return false;
    }

    // Step 2: Generate paths (hatches + perimeters)
    if (!generatePaths()) {
        std::cerr << "SlmPrint: Path generation failed" << std::endl;
        return false;
    }

    // Step 3: Export .marc file
    std::filesystem::path marcPath =
        std::filesystem::path(outputDir) / "slicefile.marc";
    if (!exportMarc(marcPath.string())) {
        std::cerr << "SlmPrint: .marc export failed" << std::endl;
        return false;
    }

    // Step 4: Export SVG layers
    if (!exportSVG(outputDir)) {
        std::cerr << "SlmPrint: SVG export failed" << std::endl;
        return false;
    }

    // Step 5: Export classified SVG layers
    if (!exportSegmentSVG(outputDir)) {
        std::cerr << "SlmPrint: Segment SVG export failed" << std::endl;
        return false;
    }

    // Step 6: Verify by reading .marc back
    MarcFile verifyFile;
    if (verifyFile.readFromFile(marcPath.string())) {
        std::cout << "SlmPrint: Verified .marc file: "
                  << verifyFile.layers.size() << " layers" << std::endl;
    }

    reportProgress("Pipeline complete", 100);
    return true;
}

bool SlmPrint::processAndExportBuildPlate(const std::string& outputDir) {
    reportProgress("Starting build plate pipeline...", 0);

    // Step 1: Process build plate (slice, surface detect, support, anchors)
    if (!processBuildPlate()) {
        std::cerr << "SlmPrint: Build plate processing failed" << std::endl;
        return false;
    }

    // Step 2: Generate paths (hatches + perimeters)
    if (!generatePaths()) {
        std::cerr << "SlmPrint: Path generation failed" << std::endl;
        return false;
    }

    // Step 3: Export .marc file
    std::filesystem::path marcPath =
        std::filesystem::path(outputDir) / "slicefile.marc";
    if (!exportMarc(marcPath.string())) {
        std::cerr << "SlmPrint: .marc export failed" << std::endl;
        return false;
    }

    // Step 4: Export SVG layers
    if (!exportSVG(outputDir)) {
        std::cerr << "SlmPrint: SVG export failed" << std::endl;
        return false;
    }

    // Step 5: Export classified SVG layers
    if (!exportSegmentSVG(outputDir)) {
        std::cerr << "SlmPrint: Segment SVG export failed" << std::endl;
        return false;
    }

    // Step 6: Verify .marc
    MarcFile verifyFile;
    if (verifyFile.readFromFile(marcPath.string())) {
        std::cout << "SlmPrint: Verified .marc file: "
                  << verifyFile.layers.size() << " layers" << std::endl;
    }

    reportProgress("Build plate pipeline complete", 100);
    return true;
}

void SlmPrint::reportProgress(const char* message, int percent) {
    if (progressCb_) {
        progressCb_(message, percent);
    }
}

} // namespace MarcSLM
