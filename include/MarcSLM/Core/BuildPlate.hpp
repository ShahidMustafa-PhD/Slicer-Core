// ==============================================================================
// MarcSLM - Build Plate Preparation
// ==============================================================================
// Ported from Legacy Slic3r Print + PrintObject classes.
// Orchestrates multi-model build plate: placement, slicing, layer generation,
// anchor/support insertion, surface classification, and infill preparation.
// ==============================================================================

#pragma once

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
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace MarcSLM {

// ==============================================================================
// Forward Declarations
// ==============================================================================

class BuildPlate;
class PrintObject;
class PrintRegion;
class BuildLayer;
class BuildLayerRegion;

// ==============================================================================
// Processing Step Enums (ported from Legacy PrintStep / PrintObjectStep)
// ==============================================================================

/// @brief Top-level build plate processing steps.
enum class BuildStep : uint8_t {
    Arrangement,     ///< Arrange objects on build plate
    Validation,      ///< Validate configuration and geometry
};

/// @brief Per-object processing steps.
enum class ObjectStep : uint8_t {
    LayerGeneration, ///< Generate Z-layer heights
    Slicing,         ///< Slice mesh at layer heights
    SurfaceDetection,///< Detect top/bottom/internal surfaces
    InfillPrep,      ///< Prepare infill regions
    Infill,          ///< Generate infill
    Anchors,         ///< Generate anchor layers
    SupportMaterial, ///< Generate support structures
};

// ==============================================================================
// Step State Tracker (ported from Legacy PrintState)
// ==============================================================================

/// @brief Tracks which processing steps have been started/completed.
/// @details Thread-safe state tracker for idempotent pipeline operations.
template <typename StepType>
class StepState {
public:
    [[nodiscard]] bool isStarted(StepType step) const {
        return started_.count(step) > 0;
    }
    [[nodiscard]] bool isDone(StepType step) const {
        return done_.count(step) > 0;
    }
    void setStarted(StepType step) {
        started_.insert(step);
    }
    void setDone(StepType step) {
        done_.insert(step);
    }
    bool invalidate(StepType step) {
        bool invalidated = started_.erase(step) > 0;
        done_.erase(step);
        return invalidated;
    }
    bool invalidateAll() {
        bool invalidated = !started_.empty();
        started_.clear();
        done_.clear();
        return invalidated;
    }

private:
    std::set<StepType> started_;
    std::set<StepType> done_;
};

// ==============================================================================
// Surface Type Classification
// ==============================================================================

/// @brief Classification of slice surfaces for SLM processing.
enum class SurfaceType : uint8_t {
    Internal       = 0,    ///< Interior solid region
    Top            = 1,    ///< Top-facing (upskin) surface
    Bottom         = 2,    ///< Bottom-facing (downskin) surface
    InternalSolid  = 3,    ///< Internal region requiring solid infill
    InternalVoid   = 4,    ///< Internal void (sparse infill)
    Bridge         = 5,    ///< Bridge surface (spans unsupported gap)
    Support        = 6,    ///< Support material region
    Anchor         = 7,    ///< Anchor/raft layer
};

// ==============================================================================
// Classified Surface (ExPolygon + type)
// ==============================================================================

/// @brief A surface polygon with its classification type.
struct ClassifiedSurface {
    Clipper2Lib::Path64  contour;   ///< Outer boundary (CCW)
    Clipper2Lib::Paths64 holes;     ///< Internal voids (CW each)
    SurfaceType          type = SurfaceType::Internal;
    uint16_t             extraPerimeters = 0;
    float                thickness = 0.0f;

    ClassifiedSurface() = default;
    ClassifiedSurface(ClassifiedSurface&&) noexcept = default;
    ClassifiedSurface& operator=(ClassifiedSurface&&) noexcept = default;
    ClassifiedSurface(const ClassifiedSurface&) = default;
    ClassifiedSurface& operator=(const ClassifiedSurface&) = default;

    [[nodiscard]] bool isValid() const noexcept { return contour.size() >= 3; }
    [[nodiscard]] bool isSolid() const noexcept {
        return type == SurfaceType::Top || type == SurfaceType::Bottom ||
               type == SurfaceType::InternalSolid;
    }
    [[nodiscard]] bool isTop() const noexcept { return type == SurfaceType::Top; }
    [[nodiscard]] bool isBottom() const noexcept {
        return type == SurfaceType::Bottom || type == SurfaceType::Bridge;
    }
    [[nodiscard]] bool isInternal() const noexcept {
        return type == SurfaceType::Internal || type == SurfaceType::InternalSolid ||
               type == SurfaceType::InternalVoid;
    }
    [[nodiscard]] bool isBridge() const noexcept { return type == SurfaceType::Bridge; }
};

/// @brief Collection of classified surfaces.
using SurfaceCollection = std::vector<ClassifiedSurface>;

// ==============================================================================
// Build Layer Region (ported from Legacy LayerRegion)
// ==============================================================================

/// @brief Per-region data within a single build layer.
/// @details Each region corresponds to a distinct set of processing parameters.
///          Contains slice polygons, classified surfaces, and fill data.
class BuildLayerRegion {
public:
    BuildLayerRegion(BuildLayer* layer, PrintRegion* region);

    [[nodiscard]] BuildLayer*   layer()  const noexcept { return layer_; }
    [[nodiscard]] PrintRegion*  region() const noexcept { return region_; }

    /// @brief Raw slice polygons (ExPolygon contour + holes).
    SurfaceCollection slices;

    /// @brief Classified fill surfaces (top/bottom/internal/bridge).
    SurfaceCollection fillSurfaces;

    /// @brief Support material surfaces for this region.
    SurfaceCollection supportSurfaces;

    /// @brief Prepare fill surfaces from classified slices.
    void prepareFillSurfaces();

private:
    BuildLayer*  layer_  = nullptr;
    PrintRegion* region_ = nullptr;
};

// ==============================================================================
// Build Layer (ported from Legacy Layer)
// ==============================================================================

/// @brief A single Z-layer in the build, containing per-region data.
/// @details Manages layer-level geometry: merged slices, per-region surfaces,
///          support fills, and layer connectivity (upper/lower neighbors).
class BuildLayer {
public:
    BuildLayer(size_t id, PrintObject* object,
               double height, double printZ, double sliceZ);
    ~BuildLayer();

    // --- Identification ---
    [[nodiscard]] size_t       id()      const noexcept { return id_; }
    [[nodiscard]] PrintObject* object()  const noexcept { return object_; }
    [[nodiscard]] double       height()  const noexcept { return height_; }
    [[nodiscard]] double       printZ()  const noexcept { return printZ_; }
    [[nodiscard]] double       sliceZ()  const noexcept { return sliceZ_; }

    void setId(size_t id)            { id_ = id; }
    void setPrintZ(double z)         { printZ_ = z; }

    // --- Layer Connectivity ---
    BuildLayer* upperLayer = nullptr;
    BuildLayer* lowerLayer = nullptr;

    // --- Error Tracking ---
    bool slicingErrors = false;

    // --- Region Access ---
    [[nodiscard]] size_t regionCount() const { return regions_.size(); }

    BuildLayerRegion* addRegion(PrintRegion* region);
    BuildLayerRegion* getRegion(size_t idx);
    const BuildLayerRegion* getRegion(size_t idx) const;

    [[nodiscard]] const std::vector<BuildLayerRegion*>& regions() const {
        return regions_;
    }

    // --- Merged Slice Data ---
    /// @brief Union of all region slices for this layer.
    Clipper2Lib::Paths64 mergedSlices;

    /// @brief Detect surface types (top/bottom/internal) by comparing
    ///        with upper and lower layers.
    void detectSurfaceTypes();

    /// @brief Merge all region slices into mergedSlices.
    void makeSlices();

private:
    size_t       id_;
    PrintObject* object_;
    double       height_;   ///< Layer thickness [mm]
    double       printZ_;   ///< Absolute Z-position [mm]
    double       sliceZ_;   ///< Slice Z (midpoint) [mm]

    std::vector<BuildLayerRegion*> regions_;
};

// ==============================================================================
// Print Region (ported from Legacy PrintRegion)
// ==============================================================================

/// @brief A region of the build sharing identical processing parameters.
/// @details Multiple volumes/objects may share a region if their configs match.
class PrintRegion {
public:
    explicit PrintRegion(BuildPlate* plate);
    ~PrintRegion() = default;

    SlmConfig config;   ///< Configuration for this region

    [[nodiscard]] BuildPlate* buildPlate() const noexcept { return plate_; }

private:
    BuildPlate* plate_ = nullptr;
};

// ==============================================================================
// Print Object (ported from Legacy PrintObject)
// ==============================================================================

/// @brief A single 3D model placed on the build plate.
/// @details Manages per-object slicing, layer generation, anchor layers,
///          support material, and surface classification.
///
/// Ported from Legacy Slic3r::PrintObject with adaptations for:
/// - Assimp-based mesh loading (vs. legacy admesh STL loading)
/// - TriMesh repair and slicing (matching legacy algorithm)
/// - SlmConfig (vs. legacy PrintConfig/PrintRegionConfig)
class PrintObject {
public:
    PrintObject(BuildPlate* plate, const InternalModel& modelDesc,
                Geometry::MeshProcessor& meshProcessor);
    ~PrintObject();

    PrintObject(const PrintObject&) = delete;
    PrintObject& operator=(const PrintObject&) = delete;

    // --- Configuration ---
    SlmConfig config;

    // --- Region Volumes ---
    /// @brief Map of region_id ? vector of volume IDs.
    std::map<size_t, std::vector<int>> regionVolumes;

    // --- Layer Access ---
    [[nodiscard]] size_t layerCount() const { return layers_.size(); }
    [[nodiscard]] size_t totalLayerCount() const {
        return layers_.size() + supportLayers_.size();
    }

    BuildLayer* getLayer(size_t idx);
    const BuildLayer* getLayer(size_t idx) const;
    BuildLayer* addLayer(size_t id, double height, double printZ, double sliceZ);
    void deleteLayer(size_t idx);
    void clearLayers();

    // --- Support Layers ---
    [[nodiscard]] size_t supportLayerCount() const { return supportLayers_.size(); }
    void clearSupportLayers();

    // --- Pipeline Steps ---

    /// @brief Generate Z-layer heights based on config.
    /// @return Vector of Z-coordinates for slicing.
    std::vector<double> generateObjectLayers(double firstLayerHeight);

    /// @brief Slice the mesh at computed Z-heights.
    void slice();

    /// @brief Add anchor/raft layers below the object.
    void addAnchorLayers();

    /// @brief Detect surface types (top/bottom/internal).
    void detectSurfaceTypes();

    /// @brief Prepare infill regions.
    void prepareInfill();

    /// @brief Generate support structures.
    void generateSupportMaterial();

    // --- State ---
    StepState<ObjectStep> state;

    // --- Queries ---
    [[nodiscard]] BuildPlate* buildPlate() const noexcept { return plate_; }
    [[nodiscard]] const Geometry::BBox3f& boundingBox() const { return bbox_; }
    [[nodiscard]] bool hasSupportMaterial() const;

    /// @brief Invalidate a specific step and its dependents.
    bool invalidateStep(ObjectStep step);
    bool invalidateAllSteps();

    // --- Placement ---
    struct Placement {
        double x = 0.0;   ///< X offset on build plate [mm]
        double y = 0.0;   ///< Y offset on build plate [mm]
        double z = 0.0;   ///< Z offset on build plate [mm]
        double roll = 0.0;
        double pitch = 0.0;
        double yaw = 0.0;
    };

    Placement placement;

    // --- Object Size (scaled) ---
    double sizeX = 0.0, sizeY = 0.0, sizeZ = 0.0;

private:
    BuildPlate*               plate_;
    Geometry::MeshProcessor*  meshProcessor_;
    Geometry::BBox3f          bbox_;
    InternalModel             modelDesc_;

    std::vector<BuildLayer*>  layers_;
    std::vector<BuildLayer*>  supportLayers_;

    // --- Internal Slicing ---
    void sliceInternal();
    std::vector<Geometry::ExPolygons2i> sliceRegion(
        size_t regionId, const std::vector<float>& zHeights);

    // --- Overhang Detection (from Legacy) ---
    struct SupportPillar {
        double x = 0.0, y = 0.0;   ///< XY position [mm]
        size_t topLayer = 0;
        size_t bottomLayer = 0;
        double radiusBase = 0.0;
        double radiusTop = 0.0;
    };

    void detectOverhangs(
        std::map<size_t, std::map<size_t, Clipper2Lib::Paths64>>& overhangAreas,
        double supportAngle);
    void generatePillarPositions(
        const std::map<size_t, std::map<size_t, Clipper2Lib::Paths64>>& overhangAreas,
        std::map<size_t, std::vector<Clipper2Lib::Point64>>& pillarPositions,
        double pillarSpacing);
    void createSupportPillars(
        const std::map<size_t, std::vector<Clipper2Lib::Point64>>& pillarPositions,
        double pillarSize);
    void generateSupportGeometry(
        const std::map<size_t, std::vector<SupportPillar>>& pillarsByRegion,
        double pillarBaseSize);
};

// ==============================================================================
// Build Plate (ported from Legacy Print)
// ==============================================================================

/// @brief The complete SLM build plate with multiple objects.
/// @details Orchestrates the full build preparation pipeline:
///          1. Add models (from InternalModel descriptors)
///          2. Configure (apply SlmConfig)
///          3. Process (slice, detect surfaces, prepare infill, support)
///          4. Export (layers for path generation)
///
/// Ported from Legacy Slic3r::Print with adaptations for the SLM process.
class BuildPlate {
public:
    using ProgressCallback = std::function<void(const char*, int)>;

    BuildPlate();
    ~BuildPlate();

    BuildPlate(const BuildPlate&) = delete;
    BuildPlate& operator=(const BuildPlate&) = delete;

    // =========================================================================
    // Configuration
    // =========================================================================

    /// @brief Master SLM configuration for the build.
    SlmConfig config;

    /// @brief Apply SLM configuration to all objects and regions.
    void applyConfig(const SlmConfig& slmConfig);

    // =========================================================================
    // Model Management
    // =========================================================================

    /// @brief Add a model to the build plate from an InternalModel descriptor.
    /// @param model Model descriptor with path, position, orientation.
    /// @return Pointer to the created PrintObject, or nullptr on failure.
    PrintObject* addModel(const InternalModel& model);

    /// @brief Add multiple models at once.
    /// @param models Vector of model descriptors.
    /// @return Number of models successfully added.
    size_t addModels(const std::vector<InternalModel>& models);

    /// @brief Remove all objects from the build plate.
    void clearObjects();

    /// @brief Delete a specific object.
    void deleteObject(size_t idx);

    /// @brief Get a specific print object.
    PrintObject* getObject(size_t idx);
    const PrintObject* getObject(size_t idx) const;

    /// @brief Number of objects on the build plate.
    [[nodiscard]] size_t objectCount() const { return objects_.size(); }

    // =========================================================================
    // Region Management
    // =========================================================================

    PrintRegion* addRegion();
    PrintRegion* getRegion(size_t idx);
    const PrintRegion* getRegion(size_t idx) const;
    [[nodiscard]] size_t regionCount() const { return regions_.size(); }

    // =========================================================================
    // Processing Pipeline
    // =========================================================================

    /// @brief Run the full processing pipeline for all objects.
    /// @details Executes: slice ? detect surfaces ? prepare infill ?
    ///          generate support ? add anchors
    void process();

    /// @brief Validate the build configuration.
    /// @throws std::runtime_error on validation failure.
    void validate() const;

    // =========================================================================
    // Output: Convert to Marc::Layer format
    // =========================================================================

    /// @brief Convert all processed layers to Marc::Layer format.
    /// @details Merges layers from all objects at matching Z-heights.
    /// @return Vector of Marc::Layer for export pipeline.
    [[nodiscard]] std::vector<Marc::Layer> exportLayers() const;

    // =========================================================================
    // Bounding Box
    // =========================================================================

    /// @brief Bounding box of all placed objects.
    [[nodiscard]] Geometry::BBox3f boundingBox() const;

    // =========================================================================
    // State
    // =========================================================================

    StepState<BuildStep> state;

    bool invalidateStep(BuildStep step);
    bool invalidateAllSteps();

    // =========================================================================
    // Progress
    // =========================================================================

    void setProgressCallback(ProgressCallback cb) { progressCb_ = std::move(cb); }

private:
    std::vector<PrintObject*> objects_;
    std::vector<PrintRegion*> regions_;

    /// @brief Per-model mesh processors (owns loaded meshes).
    std::vector<std::unique_ptr<Geometry::MeshProcessor>> meshProcessors_;

    ProgressCallback progressCb_;

    void reportProgress(const char* msg, int pct);
    void clearRegions();
};

} // namespace MarcSLM
