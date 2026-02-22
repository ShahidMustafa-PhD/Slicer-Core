// ==============================================================================
// MarcSLM - Top-Level API Implementation
// ==============================================================================
// Ported from Legacy MarcAPI.cpp
// ==============================================================================

#include "MarcSLM/MarcAPI.hpp"
#include "MarcSLM/Core/ErrorState.hpp"

#include <iostream>
#include <filesystem>

namespace MarcSLM {

MarcAPI::MarcAPI(float bed_width, float bed_depth, float spacing)
    : bed_width_(bed_width)
    , bed_depth_(bed_depth)
    , spacing_(spacing)
    , slmPrint_(std::make_unique<SlmPrint>()) {
    // Pass initial bed dimensions to the build plate.
    // These will be overridden by values from the JSON config file once
    // loadSlmConfig() is called (via bedWidth / bedDepth keys).
    slmPrint_->buildPlate().setBedSize(bed_width, bed_depth);
}

MarcAPI::~MarcAPI() = default;

// =========================================================================
// Model Management
// =========================================================================

MarcErrorCode MarcAPI::setModels(const std::vector<InternalModel>& models) {
    try {
        models_ = models;
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Exception in setModels: " + std::string(e.what()));
        return MARC_E_FAIL;
    }

    modelPaths_.clear();
    for (const auto& model : models) {
        try {
            if (!std::filesystem::exists(model.path)) {
                std::cerr << "Model file does not exist: " << model.path << std::endl;
                return MARC_E_FAIL;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
            return MARC_E_FAIL;
        }
        modelPaths_.push_back(model.path);
    }

    if (models.empty()) {
        std::cerr << "No models provided." << std::endl;
        return MARC_E_FAIL;
    }

    // Load build configuration from first model
    try {
        if (!std::filesystem::exists(models[0].buildconfig)) {
            std::cerr << "BuildConfiguration file does not exist: "
                      << models[0].buildconfig << std::endl;
            return MARC_E_FAIL;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return MARC_E_FAIL;
    }

    configFileJson_ = models[0].buildconfig;
    return MARC_S_OK;
}

std::vector<InternalModel> MarcAPI::getInternalModels() const {
    return models_;
}

// =========================================================================
// Configuration
// =========================================================================

MarcErrorCode MarcAPI::set_config_json(const std::filesystem::path& filePath) {
    if (filePath.empty()) return MARC_E_FAIL;

    try {
        if (!std::filesystem::exists(filePath)) return MARC_E_FAIL;
    } catch (...) {
        return MARC_E_FAIL;
    }

    configFileJson_ = filePath;
    return MARC_S_OK;
}

MarcErrorCode MarcAPI::updateModel() {
    try {
        // Load configuration
        if (!configFileJson_.empty()) {
            if (!slmPrint_->loadSlmConfig(configFileJson_)) {
                std::cerr << "MarcAPI: Failed to load SlmConfig" << std::endl;
                return MARC_E_FAIL;
            }
        }

        // Load the first model (primary model)
        if (models_.empty()) {
            std::cerr << "MarcAPI: No models set" << std::endl;
            return MARC_E_FAIL;
        }

        // Load mesh via Assimp ? TriMesh
        if (!slmPrint_->loadMesh(models_[0].path)) {
            std::cerr << "MarcAPI: Failed to load mesh" << std::endl;
            return MARC_E_FAIL;
        }

        return MARC_S_OK;
    } catch (const std::exception& e) {
        std::cerr << "MarcAPI: " << e.what() << std::endl;
        return MARC_E_FAIL;
    }
}

// =========================================================================
// Geometric Transforms (stubs — mesh transforms applied via Manifold)
// =========================================================================

MarcErrorCode MarcAPI::rotateX(float /*angleDeg*/) {
    // TODO: Apply rotation to the loaded mesh
    return MARC_S_OK;
}

MarcErrorCode MarcAPI::rotateY(float /*angleDeg*/) {
    return MARC_S_OK;
}

MarcErrorCode MarcAPI::rotateZ(float /*angleDeg*/) {
    return MARC_S_OK;
}

MarcErrorCode MarcAPI::scale(float /*factor*/) {
    return MARC_S_OK;
}

MarcErrorCode MarcAPI::alignXY(float /*px*/, float /*py*/) {
    return MARC_S_OK;
}

// =========================================================================
// Pipeline Execution (Single-Model Legacy Path)
// =========================================================================

MarcErrorCode MarcAPI::exportSlmFile() {
    // Step 1: Update model (load mesh + config)
    if (updateModel() != MARC_S_OK) {
        std::cerr << "MarcAPI: Failed to update model" << std::endl;
        return MARC_E_FAIL;
    }

    // Determine output directory
    std::string outputDir;
    if (!modelPaths_.empty()) {
        outputDir = std::filesystem::path(models_.back().path)
                        .parent_path().string();
    } else {
        outputDir = ".";
    }

    // Step 2: Run the full pipeline
    try {
        if (!slmPrint_->processAndExport(outputDir)) {
            std::cerr << "MarcAPI: Pipeline failed" << std::endl;
            return MARC_E_FAIL;
        }
    } catch (const std::exception& e) {
        std::cerr << "MarcAPI: " << e.what() << std::endl;
        return MARC_E_FAIL;
    }

    std::cout << "MarcAPI: SLM export complete" << std::endl;
    return MARC_S_OK;
}

// =========================================================================
// Pipeline Execution (Build Plate Multi-Model Path)
// =========================================================================

MarcErrorCode MarcAPI::exportSlmFileBuildPlate() {
    try {
        // Step 1: Load configuration
        if (!configFileJson_.empty()) {
            if (!slmPrint_->loadSlmConfig(configFileJson_)) {
                std::cerr << "MarcAPI: Failed to load SlmConfig" << std::endl;
                return MARC_E_FAIL;
            }
        }

        if (models_.empty()) {
            std::cerr << "MarcAPI: No models set" << std::endl;
            return MARC_E_FAIL;
        }

        // Step 2: Add all models to build plate
        size_t added = slmPrint_->addModelsToBuildPlate(models_);
        if (added == 0) {
            std::cerr << "MarcAPI: Failed to add models to build plate" << std::endl;
            return MARC_E_FAIL;
        }
        std::cout << "MarcAPI: Added " << added << " of " << models_.size()
                  << " models to build plate" << std::endl;

        // Determine output directory
        std::string outputDir = std::filesystem::path(models_.back().path)
                                    .parent_path().string();
        if (outputDir.empty()) outputDir = ".";

        // Step 3: Run the full build plate pipeline
        if (!slmPrint_->processAndExportBuildPlate(outputDir)) {
            std::cerr << "MarcAPI: Build plate pipeline failed" << std::endl;
            return MARC_E_FAIL;
        }

        std::cout << "MarcAPI: Build plate SLM export complete" << std::endl;
        return MARC_S_OK;

    } catch (const std::exception& e) {
        std::cerr << "MarcAPI: " << e.what() << std::endl;
        return MARC_E_FAIL;
    }
}

MarcErrorCode MarcAPI::arrangeBuildPlate() {
    // TODO: Implement arrangement using bounding-box packing
    return MARC_S_OK;
}

// =========================================================================
// Build Plate Access
// =========================================================================

BuildPlate& MarcAPI::getBuildPlate() {
    return slmPrint_->buildPlate();
}

const BuildPlate& MarcAPI::getBuildPlate() const {
    return slmPrint_->buildPlate();
}

// =========================================================================
// Progress
// =========================================================================

void MarcAPI::registerProgressCallback(ProgressCallback cb) {
    progressCb_ = std::move(cb);
    if (slmPrint_) {
        slmPrint_->setProgressCallback(progressCb_);
    }
}

void MarcAPI::reportProgress(const char* message, int percent) {
    if (progressCb_) {
        progressCb_(message, percent);
    }
}

} // namespace MarcSLM
