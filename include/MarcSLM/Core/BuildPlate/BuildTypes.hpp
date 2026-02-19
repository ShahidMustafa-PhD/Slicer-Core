// ==============================================================================
// MarcSLM - BuildPlate Shared Types
// ==============================================================================
// This header is the single shared-type contract for all BuildPlate sub-modules.
// It re-exports the types from BuildPlate.hpp that are needed by the specialist
// classes without pulling in the full BuildPlate orchestrator.
//
// Include order inside the BuildPlate subsystem:
//   BuildTypes.hpp   ? LayerHeightGenerator / SurfaceClassifier /
//                       OverhangDetector / SupportGenerator / LayerExporter
//   BuildPlate.hpp   ? public API (includes BuildTypes.hpp transitively)
// ==============================================================================

#pragma once

// Forward-declare everything that lives in MarcSLM::  so each specialist
// header only needs to include this one file.

#include "MarcSLM/Core/MarcFormat.hpp"
#include "MarcSLM/Geometry/TriMesh.hpp"   // MESH_SCALING_FACTOR, BBox3f

#include <clipper2/clipper.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace MarcSLM {

// ==============================================================================
// Forward Declarations (avoid pulling in full headers in sub-module headers)
// ==============================================================================

class BuildPlate;
class PrintObject;
class PrintRegion;
class BuildLayer;
class BuildLayerRegion;

// ==============================================================================
// Processing Step Enumerations
// ==============================================================================

/// @brief Top-level build-plate processing steps.
enum class BuildStep : std::uint8_t {
    Arrangement,   ///< Arrange objects on the build plate
    Validation,    ///< Validate configuration and geometry
};

/// @brief Per-object processing steps.
enum class ObjectStep : std::uint8_t {
    LayerGeneration,   ///< Generate Z-layer heights
    Slicing,           ///< Slice mesh at layer heights
    SurfaceDetection,  ///< Detect top/bottom/internal surfaces
    InfillPrep,        ///< Prepare infill regions
    Infill,            ///< Generate infill
    Anchors,           ///< Generate anchor layers
    SupportMaterial,   ///< Generate support structures
};

// ==============================================================================
// Surface Type Classification
// ==============================================================================

/// @brief Classification of slice surfaces for SLM thermal processing.
enum class SurfaceType : std::uint8_t {
    Internal      = 0,  ///< Interior solid region
    Top           = 1,  ///< Top-facing (upskin) surface
    Bottom        = 2,  ///< Bottom-facing (downskin) surface
    InternalSolid = 3,  ///< Internal region requiring solid infill
    InternalVoid  = 4,  ///< Internal void (sparse infill)
    Bridge        = 5,  ///< Bridge surface spanning unsupported gap
    Support       = 6,  ///< Support material region
    Anchor        = 7,  ///< Anchor / raft layer
};

// ==============================================================================
// ClassifiedSurface  (ExPolygon + classification tag)
// ==============================================================================

/// @brief A surface polygon with its SLM classification type.
struct ClassifiedSurface {
    Clipper2Lib::Path64  contour;          ///< Outer boundary (CCW)
    Clipper2Lib::Paths64 holes;            ///< Internal voids (CW each)
    SurfaceType          type            = SurfaceType::Internal;
    std::uint16_t        extraPerimeters = 0;
    float                thickness       = 0.0f;

    ClassifiedSurface() = default;
    ClassifiedSurface(ClassifiedSurface&&) noexcept            = default;
    ClassifiedSurface& operator=(ClassifiedSurface&&) noexcept = default;
    ClassifiedSurface(const ClassifiedSurface&)                = default;
    ClassifiedSurface& operator=(const ClassifiedSurface&)     = default;

    [[nodiscard]] bool isValid()    const noexcept { return contour.size() >= 3; }
    [[nodiscard]] bool isSolid()    const noexcept {
        return type == SurfaceType::Top           ||
               type == SurfaceType::Bottom        ||
               type == SurfaceType::InternalSolid;
    }
    [[nodiscard]] bool isTop()      const noexcept { return type == SurfaceType::Top; }
    [[nodiscard]] bool isBottom()   const noexcept {
        return type == SurfaceType::Bottom || type == SurfaceType::Bridge;
    }
    [[nodiscard]] bool isInternal() const noexcept {
        return type == SurfaceType::Internal      ||
               type == SurfaceType::InternalSolid ||
               type == SurfaceType::InternalVoid;
    }
    [[nodiscard]] bool isBridge()   const noexcept { return type == SurfaceType::Bridge; }
};

/// @brief Ordered collection of classified surfaces.
using SurfaceCollection = std::vector<ClassifiedSurface>;

// ==============================================================================
// BuildLayerRegion  (per-region data for one Z-layer)
// ==============================================================================

/// @brief Per-region data within a single build layer.
class BuildLayerRegion {
public:
    explicit BuildLayerRegion(BuildLayer* layer, PrintRegion* region) noexcept;

    [[nodiscard]] BuildLayer*  layer()  const noexcept { return layer_;  }
    [[nodiscard]] PrintRegion* region() const noexcept { return region_; }

    /// @brief Raw slice polygons (contour + holes from the mesh slicer).
    SurfaceCollection slices;

    /// @brief Classified fill surfaces (top / bottom / internal / bridge).
    SurfaceCollection fillSurfaces;

    /// @brief Support material surfaces (pillars stamped by SupportGenerator).
    SurfaceCollection supportSurfaces;

    /// @brief Copy slices ? fillSurfaces when fillSurfaces is empty.
    void prepareFillSurfaces();

private:
    BuildLayer*  layer_  = nullptr;
    PrintRegion* region_ = nullptr;
};

// ==============================================================================
// BuildLayer  (one Z-layer in the build)
// ==============================================================================

/// @brief A single Z-layer holding per-region geometry.
class BuildLayer {
public:
    BuildLayer(std::size_t id, PrintObject* object,
               double height, double printZ, double sliceZ);
    ~BuildLayer();

    // Non-copyable, non-movable (owned by PrintObject via raw ptr vector)
    BuildLayer(const BuildLayer&)            = delete;
    BuildLayer& operator=(const BuildLayer&) = delete;
    BuildLayer(BuildLayer&&)                 = delete;
    BuildLayer& operator=(BuildLayer&&)      = delete;

    // --- Identification ---
    [[nodiscard]] std::size_t  id()     const noexcept { return id_;     }
    [[nodiscard]] PrintObject* object() const noexcept { return object_; }
    [[nodiscard]] double       height() const noexcept { return height_; }
    [[nodiscard]] double       printZ() const noexcept { return printZ_; }
    [[nodiscard]] double       sliceZ() const noexcept { return sliceZ_; }

    void setId(std::size_t id)   noexcept { id_     = id; }
    void setPrintZ(double z)     noexcept { printZ_ = z;  }

    // --- Layer Connectivity (non-owning neighbours) ---
    BuildLayer* upperLayer = nullptr;
    BuildLayer* lowerLayer = nullptr;

    bool slicingErrors = false;

    // --- Region Access ---
    [[nodiscard]] std::size_t regionCount() const noexcept {
        return regions_.size();
    }

    BuildLayerRegion*       addRegion(PrintRegion* region);
    BuildLayerRegion*       getRegion(std::size_t idx)       noexcept;
    const BuildLayerRegion* getRegion(std::size_t idx) const noexcept;

    [[nodiscard]] const std::vector<BuildLayerRegion*>& regions() const noexcept {
        return regions_;
    }

    // --- Merged Slices ---
    /// @brief Union of all region contours for this layer.
    Clipper2Lib::Paths64 mergedSlices;

    /// @brief Merge all region slices into mergedSlices via Clipper2 Union.
    void makeSlices();

private:
    std::size_t  id_;
    PrintObject* object_;
    double       height_;   ///< Layer thickness [mm]
    double       printZ_;   ///< Absolute Z-position [mm]
    double       sliceZ_;   ///< Slice Z midpoint [mm]

    std::vector<BuildLayerRegion*> regions_;
};

} // namespace MarcSLM
