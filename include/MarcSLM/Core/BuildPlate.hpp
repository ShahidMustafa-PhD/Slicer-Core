// ==============================================================================
// MarcSLM - Build Plate Orchestrator
// ==============================================================================
// Provides the public API for adding models, configuring the build, running
// the full processing pipeline, and exporting to Marc::Layer format.
//
// Sub-tasks are delegated to specialist classes in namespace MarcSLM::BP:
//   BP::LayerHeightGenerator  — Z-height computation
//   BP::SurfaceClassifier     — top/bottom/internal detection
//   BP::OverhangDetector      — unsupported region detection
//   BP::SupportGenerator      — pillar placement and geometry
//   BP::LayerExporter         — BuildLayer -> Marc::Layer conversion
// ==============================================================================

#pragma once

// Sub-module shared types (ClassifiedSurface, BuildLayer, BuildLayerRegion, ...)
#include "MarcSLM/Core/BuildPlate/BuildTypes.hpp"

// Specialist service classes (namespace MarcSLM::BP)
#include "MarcSLM/Core/BuildPlate/LayerHeightGenerator.hpp"
#include "MarcSLM/Core/BuildPlate/SurfaceClassifier.hpp"
#include "MarcSLM/Core/BuildPlate/OverhangDetector.hpp"
#include "MarcSLM/Core/BuildPlate/SupportGenerator.hpp"
#include "MarcSLM/Core/BuildPlate/LayerExporter.hpp"
#include "MarcSLM/Core/BuildPlate/ModelPlacement.hpp"
#include "MarcSLM/Core/BuildPlate/BuildPlatePreparator.hpp"

// Domain headers
#include "MarcSLM/Core/SlmConfig.hpp"
#include "MarcSLM/Core/InternalModel.hpp"
#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"
#include "MarcSLM/Geometry/MeshProcessor.hpp"

#include <clipper2/clipper.h>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace MarcSLM {

// Forward declarations
class BuildPlate;
class PrintObject;

// ==============================================================================
// StepState - idempotent pipeline state tracker
// ==============================================================================

/// @brief Tracks which processing steps have been started/completed.
template <typename StepType>
class StepState {
public:
    [[nodiscard]] bool isStarted(StepType s) const { return started_.count(s) > 0; }
    [[nodiscard]] bool isDone   (StepType s) const { return done_.count(s)    > 0; }
    void setStarted(StepType s) { started_.insert(s); }
    void setDone   (StepType s) { done_.insert(s);    }
    bool invalidate(StepType s) {
        bool changed = started_.erase(s) > 0;
        done_.erase(s);
        return changed;
    }
    bool invalidateAll() {
        bool changed = !started_.empty();
        started_.clear();
        done_.clear();
        return changed;
    }
private:
    std::set<StepType> started_;
    std::set<StepType> done_;
};

// Explicit instantiation declarations (definitions in BuildPlate.cpp)
extern template class StepState<BuildStep>;
extern template class StepState<ObjectStep>;

// ==============================================================================
// PrintRegion
// ==============================================================================

/// @brief A region of the build that shares identical processing parameters.
class PrintRegion {
public:
    explicit PrintRegion(BuildPlate* plate) noexcept;
    ~PrintRegion() = default;
    PrintRegion(const PrintRegion&)            = delete;
    PrintRegion& operator=(const PrintRegion&) = delete;

    SlmConfig config;
    [[nodiscard]] BuildPlate* buildPlate() const noexcept { return plate_; }

private:
    BuildPlate* plate_ = nullptr;
};

// ==============================================================================
// PrintObject - per-model processing unit
// ==============================================================================

/// @brief A single 3D model placed on the build plate.
///
/// Orchestrates per-object slice pipeline by delegating to BP:: specialists.
/// The class itself holds no algorithm logic - only orchestration and state.
class PrintObject {
public:
    PrintObject(BuildPlate*              plate,
                const InternalModel&     modelDesc,
                Geometry::MeshProcessor& meshProcessor);
    ~PrintObject();
    PrintObject(const PrintObject&)            = delete;
    PrintObject& operator=(const PrintObject&) = delete;

    // Configuration
    SlmConfig config;
    std::map<std::size_t, std::vector<int>> regionVolumes;

    // Layer Access
    [[nodiscard]] std::size_t layerCount()      const noexcept { return layers_.size(); }
    [[nodiscard]] std::size_t totalLayerCount() const noexcept {
        return layers_.size() + supportLayers_.size();
    }
    BuildLayer*       getLayer(std::size_t idx)       noexcept;
    const BuildLayer* getLayer(std::size_t idx) const noexcept;
    BuildLayer* addLayer(std::size_t id, double height, double printZ, double sliceZ);
    void deleteLayer(std::size_t idx);
    void clearLayers();
    [[nodiscard]] std::size_t supportLayerCount() const noexcept {
        return supportLayers_.size();
    }
    void clearSupportLayers();

    // Pipeline Steps (idempotent - guarded by StepState)
    void slice();
    void addAnchorLayers();
    void detectSurfaceTypes();
    void prepareInfill();
    void generateSupportMaterial();

    // State
    StepState<ObjectStep> state;
    [[nodiscard]] bool hasSupportMaterial() const noexcept;
    bool invalidateStep(ObjectStep step);
    bool invalidateAllSteps();

    // Placement
    struct Placement {
        double x = 0.0, y = 0.0, z = 0.0;
        double roll = 0.0, pitch = 0.0, yaw = 0.0;
    };
    Placement placement;
    double sizeX = 0.0, sizeY = 0.0, sizeZ = 0.0;

    [[nodiscard]] BuildPlate*             buildPlate()  const noexcept { return plate_; }
    [[nodiscard]] const Geometry::BBox3f& boundingBox() const noexcept { return bbox_;  }

private:
    BuildPlate*               plate_;
    Geometry::MeshProcessor*  meshProcessor_;
    Geometry::BBox3f          bbox_;
    InternalModel             modelDesc_;
    std::vector<BuildLayer*>  layers_;
    std::vector<BuildLayer*>  supportLayers_;

    // Specialist services (namespace MarcSLM::BP - stateless value types)
    BP::LayerHeightGenerator layerGen_;
    BP::SurfaceClassifier    surfaceClassifier_;
    BP::OverhangDetector     overhangDetector_;
    BP::SupportGenerator     supportGen_;

    void sliceInternal();
    std::vector<Geometry::ExPolygons2i> sliceRegion(
        std::size_t regionId, const std::vector<float>& zHeights);
};

// ==============================================================================
// BuildPlate - top-level orchestrator
// ==============================================================================

/// @brief The complete SLM build plate holding one or more objects.
///
/// Pipeline:
///   1. addModel() / addModels()
///   2. applyConfig()
///   3. validate()
///   4. process()      -> slice, surfaces, infill, support
///   5. exportLayers() -> Marc::Layer
class BuildPlate {
public:
    using ProgressCallback = std::function<void(const char*, int)>;

    BuildPlate();
    ~BuildPlate();
    BuildPlate(const BuildPlate&)            = delete;
    BuildPlate& operator=(const BuildPlate&) = delete;

    // Configuration
    SlmConfig config;
    void applyConfig(const SlmConfig& slmConfig);

    /// @brief Set build plate physical dimensions [mm].
    void setBedSize(float width, float depth);

    // Model Management
    PrintObject* addModel(const InternalModel& model);
    std::size_t  addModels(const std::vector<InternalModel>& models);
    void clearObjects();
    void deleteObject(std::size_t idx);
    PrintObject*       getObject(std::size_t idx)       noexcept;
    const PrintObject* getObject(std::size_t idx) const noexcept;
    [[nodiscard]] std::size_t objectCount() const noexcept { return objects_.size(); }

    // Region Management
    PrintRegion*       addRegion();
    PrintRegion*       getRegion(std::size_t idx)       noexcept;
    const PrintRegion* getRegion(std::size_t idx) const noexcept;
    [[nodiscard]] std::size_t regionCount() const noexcept { return regions_.size(); }

    // =========================================================================
    // Build Plate Preparation  (delegates to BP::BuildPlatePreparator)
    // =========================================================================

    /// @brief Prepare the build plate before slicing.
    /// @details Applies per-model placement transforms (position + Euler orientation),
    ///          validates no bounding-box overlaps, checks all models fit within the
    ///          bed, aligns everything to Z=0, and creates a unified composite mesh
    ///          that can be sliced as one monolithic object while preserving per-model
    ///          identity through region tagging.
    ///
    /// This is the SLM-specific equivalent of Legacy Slic3r's
    ///   Model::arrange_objects() + Print::apply_config() pipeline.
    ///
    /// Must be called after addModel()/addModels() and before process();
    /// @throws std::runtime_error on overlap, out-of-bed, or invalid state.
    void prepareBuildPlate();

    /// @brief Check if build plate has been prepared.
    [[nodiscard]] bool isPrepared() const noexcept { return prepared_; }

    /// @brief Auto-arrange models on the build plate to avoid collisions.
    /// @param spacing Minimum gap between parts [mm]. 0 = use config.duplicate_distance.
    /// @return true if all models were arranged successfully.
    bool arrangeModels(double spacing = 0.0);

    /// @brief Validate that no two models' bounding boxes overlap.
    /// @throws std::runtime_error if overlap is detected.
    void validateNoOverlap() const;

    /// @brief Validate that all models fit within the build plate bed.
    /// @throws std::runtime_error if any model is out of bounds.
    void validateFitsInBed() const;

    /// @brief Get the per-model placement descriptors (read-only).
    [[nodiscard]] const std::vector<BP::ModelPlacement>& placements() const noexcept {
        return placements_;
    }

    // =========================================================================
    // Processing Pipeline
    // =========================================================================

    /// @brief Run the full processing pipeline.
    /// @details Calls sliceAllUnified() to slice every model at a single shared
    ///          set of Z-heights so every output layer contains the contours of
    ///          ALL models simultaneously.  Then runs surface detection, infill
    ///          preparation, and support generation on the unified layer stack.
    void process();

    void validate() const;

    // =========================================================================
    // Unified Slicing  (SLM-correct: all models at common Z-heights)
    // =========================================================================

    /// @brief Slice ALL models at a single, shared set of Z-heights.
    ///
    /// @details
    ///   Legacy per-object slicing produces one layer stack per model and then
    ///   merges them by sorting on printZ.  Because each object has a different
    ///   height the sorted stack naturally alternates between objects, so every
    ///   SVG layer shows only one model.
    ///
    ///   This method implements the correct SLM approach:
    ///     1. Determine the tallest model → derive ONE Z-height sequence.
    ///     2. Slice EVERY model's mesh at those common heights.
    ///     3. Each resulting BuildLayer gets one BuildLayerRegion PER MODEL
    ///        (region index == object index) so per-model identity is preserved
    ///        through region tags while every layer contains all parts.
    ///     4. Models shorter than the tallest simply produce empty regions above
    ///        their own height — they contribute nothing to those layers.
    ///
    ///   The unified layer stack is stored in unifiedLayers_ and is the
    ///   authoritative source for exportLayers().
    void sliceAllUnified();

    /// @brief Access the unified layer stack produced by sliceAllUnified().
    [[nodiscard]] const std::vector<BuildLayer*>& unifiedLayers() const noexcept {
        return unifiedLayers_;
    }

    // Export
    [[nodiscard]] std::vector<Marc::Layer> exportLayers() const;

    // Bounding Box
    [[nodiscard]] Geometry::BBox3f boundingBox() const;

    // State
    StepState<BuildStep> state;
    bool invalidateStep(BuildStep step);
    bool invalidateAllSteps();

    // Progress
    void setProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }

private:
    std::vector<PrintObject*>                             objects_;
    std::vector<PrintRegion*>                             regions_;
    std::vector<std::unique_ptr<Geometry::MeshProcessor>> meshProcessors_;
    std::vector<BP::ModelPlacement>                       placements_;
    ProgressCallback progressCb_;

    float bedWidth_  = 120.0f;   ///< Build plate width [mm]
    float bedDepth_  = 120.0f;   ///< Build plate depth [mm]
    bool  prepared_  = false;    ///< Has prepareBuildPlate() been called?

    BP::BuildPlatePreparator preparator_;   ///< Delegate for preparation logic

    /// @brief Unified layer stack: one BuildLayer per Z-height, each layer
    ///        contains one BuildLayerRegion per model (indexed by object index).
    std::vector<BuildLayer*> unifiedLayers_;

    void reportProgress(const char* msg, int pct);
    void clearRegions();
    void clearUnifiedLayers();

    /// @brief Apply placement transforms to all mesh processors.
    void applyAllPlacements();

    /// @brief Check if two axis-aligned bounding boxes overlap in XY.
    [[nodiscard]] static bool bboxOverlapXY(
        const Geometry::BBox3f& a, const Geometry::BBox3f& b,
        float gap = 0.0f) noexcept;
};

} // namespace MarcSLM
