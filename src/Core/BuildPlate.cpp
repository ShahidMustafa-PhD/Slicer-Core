// ==============================================================================
// MarcSLM - Build Plate Orchestrator — Implementation
// ==============================================================================
// This file contains ONLY orchestration logic:
//   - State management & pipeline sequencing
//   - Object/region lifecycle (add, delete, clear)
//   - Delegation to specialist service classes
//
// No algorithm code lives here.  Algorithm implementations are in:
//   src/Core/BuildPlate/BuildTypes.cpp          — BuildLayer / BuildLayerRegion
//   src/Core/BuildPlate/LayerHeightGenerator.cpp
//   src/Core/BuildPlate/SurfaceClassifier.cpp
//   src/Core/BuildPlate/OverhangDetector.cpp
//   src/Core/BuildPlate/SupportGenerator.cpp
//   src/Core/BuildPlate/LayerExporter.cpp
// ==============================================================================

#include "MarcSLM/Core/BuildPlate.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace MarcSLM {

// ==============================================================================
// StepState explicit instantiations
// ==============================================================================

template class StepState<BuildStep>;
template class StepState<ObjectStep>;

// ==============================================================================
// PrintRegion
// ==============================================================================

PrintRegion::PrintRegion(BuildPlate* plate) noexcept
    : plate_(plate)
{}

// ==============================================================================
// PrintObject — Construction / Destruction
// ==============================================================================

PrintObject::PrintObject(BuildPlate*              plate,
                          const InternalModel&     modelDesc,
                          Geometry::MeshProcessor& meshProcessor)
    : plate_(plate)
    , meshProcessor_(&meshProcessor)
    , modelDesc_(modelDesc)
{
    placement.x     = modelDesc.xpos;
    placement.y     = modelDesc.ypos;
    placement.z     = modelDesc.zpos;
    placement.roll  = modelDesc.roll;
    placement.pitch = modelDesc.pitch;
    placement.yaw   = modelDesc.yaw;

    if (meshProcessor_->hasValidMesh()) {
        bbox_ = meshProcessor_->getBoundingBox();
        sizeX = static_cast<double>(bbox_.sizeX());
        sizeY = static_cast<double>(bbox_.sizeY());
        sizeZ = static_cast<double>(bbox_.sizeZ());
    }
}

PrintObject::~PrintObject()
{
    clearLayers();
    clearSupportLayers();
}

// ==============================================================================
// PrintObject — Layer Management
// ==============================================================================

BuildLayer* PrintObject::getLayer(std::size_t idx) noexcept
{
    return (idx < layers_.size()) ? layers_[idx] : nullptr;
}

const BuildLayer* PrintObject::getLayer(std::size_t idx) const noexcept
{
    return (idx < layers_.size()) ? layers_[idx] : nullptr;
}

BuildLayer* PrintObject::addLayer(std::size_t id, double height,
                                   double printZ, double sliceZ)
{
    auto* layer = new BuildLayer(id, this, height, printZ, sliceZ);
    layers_.push_back(layer);
    return layer;
}

void PrintObject::deleteLayer(std::size_t idx)
{
    if (idx >= layers_.size()) return;
    delete layers_[idx];
    layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(idx));
}

void PrintObject::clearLayers()
{
    for (auto* l : layers_) delete l;
    layers_.clear();
}

void PrintObject::clearSupportLayers()
{
    for (auto* l : supportLayers_) delete l;
    supportLayers_.clear();
}

// ==============================================================================
// PrintObject — Pipeline: Slicing (per-object, legacy path)
// ==============================================================================

void PrintObject::slice()
{
    if (state.isDone(ObjectStep::Slicing)) return;
    state.setStarted(ObjectStep::Slicing);

    std::cout << "  PrintObject: Slicing '" << modelDesc_.path << "' …\n";

    sliceInternal();

    // Drop empty layers from the bottom
    while (!layers_.empty()) {
        const auto* first = layers_.front();
        bool hasContent = false;
        for (std::size_t ri = 0; ri < first->regionCount(); ++ri) {
            const auto* r = first->getRegion(ri);
            if (r && !r->slices.empty()) { hasContent = true; break; }
        }
        if (hasContent) break;
        deleteLayer(0);
        for (std::size_t i = 0; i < layers_.size(); i++)
            layers_[i]->setId(i);
    }

    // Drop empty layers from the top
    while (!layers_.empty()) {
        const auto* last = layers_.back();
        bool hasContent = false;
        for (std::size_t ri = 0; ri < last->regionCount(); ++ri) {
            const auto* r = last->getRegion(ri);
            if (r && !r->slices.empty()) { hasContent = true; break; }
        }
        if (hasContent) break;
        deleteLayer(layers_.size() - 1);
    }

    if (layers_.empty()) {
        std::cerr << "  PrintObject: No layers generated after slicing!\n";
    } else {
        std::cout << "  PrintObject: " << layers_.size()
                  << " layers generated.\n";
    }

    state.setDone(ObjectStep::Slicing);
}

void PrintObject::sliceInternal()
{
    // --- Layer height sequence -------------------------------------------------
    const std::vector<double> zHeights = layerGen_.generate(
        config, sizeZ, config.first_layer_thickness);

    clearLayers();
    if (zHeights.empty()) return;

    // --- Build layer objects and Z-list for the slicer -------------------------
    std::vector<float> sliceZs;
    sliceZs.reserve(zHeights.size());

    BuildLayer* prev  = nullptr;
    double      lo    = 0.0;
    double      hi    = 0.0;
    std::size_t id    = 0;

    for (const double zTop : zHeights) {
        lo = hi;
        hi = zTop;
        const double sliceZ = 0.5 * (lo + hi);

        auto* layer = addLayer(id++, hi - lo, hi, sliceZ);
        sliceZs.push_back(static_cast<float>(sliceZ));

        if (prev) {
            prev->upperLayer = layer;
            layer->lowerLayer = prev;
        }

        // Attach regions
        for (std::size_t ri = 0; ri < plate_->regionCount(); ++ri) {
            layer->addRegion(plate_->getRegion(ri));
        }

        prev = layer;
    }

    // --- Mesh slicing ----------------------------------------------------------
    if (!meshProcessor_->hasValidMesh()) {
        std::cerr << "  PrintObject: No valid mesh — skipping slice.\n";
        return;
    }

    const auto exPolygonsByLayer = meshProcessor_->mesh_->slice(sliceZs);

    // --- Distribute results to layer regions -----------------------------------
    for (std::size_t li = 0;
         li < exPolygonsByLayer.size() && li < layers_.size();
         ++li)
    {
        auto* layer = layers_[li];
        if (layer->regionCount() == 0) continue;

        auto* regionLayer = layer->getRegion(0);
        if (!regionLayer) continue;

        for (const auto& exPoly : exPolygonsByLayer[li]) {
            ClassifiedSurface surf;
            surf.contour.reserve(exPoly.contour.size());

            for (const auto& pt : exPoly.contour)
                surf.contour.emplace_back(pt.x, pt.y);

            for (const auto& hole : exPoly.holes) {
                Clipper2Lib::Path64 hp;
                hp.reserve(hole.size());
                for (const auto& pt : hole) hp.emplace_back(pt.x, pt.y);
                surf.holes.push_back(std::move(hp));
            }

            surf.type = SurfaceType::Internal;
            regionLayer->slices.push_back(std::move(surf));
        }
    }

    // --- Merge per-layer slices ------------------------------------------------
    for (auto* layer : layers_) {
        layer->makeSlices();
    }
}

std::vector<Geometry::ExPolygons2i> PrintObject::sliceRegion(
    std::size_t /*regionId*/, const std::vector<float>& zHeights)
{
    if (!meshProcessor_->hasValidMesh()) return {};
    return meshProcessor_->mesh_->slice(zHeights);
}

// ==============================================================================
// PrintObject — Pipeline: Anchor Layers
// ==============================================================================

void PrintObject::addAnchorLayers()
{
    if (state.isDone(ObjectStep::Anchors)) return;
    state.setStarted(ObjectStep::Anchors);

    const double anchorThickness = config.anchors_layer_thickness;
    const double totalAnchorH    = config.anchors;

    if (totalAnchorH <= 0.0) {
        state.setDone(ObjectStep::Anchors);
        return;
    }

    const int numAnchorLayers =
        std::max(1, static_cast<int>(totalAnchorH / anchorThickness));

    std::cout << "  PrintObject: Inserting " << numAnchorLayers
              << " anchor layers.\n";

    // Stash existing layers, then rebuild with anchors prepended
    std::vector<BuildLayer*> original;
    original.reserve(layers_.size());
    original.swap(layers_);

    layers_.reserve(static_cast<std::size_t>(numAnchorLayers) + original.size());

    BuildLayer* prev       = nullptr;
    double      currentZ   = 0.0;

    for (int i = 0; i < numAnchorLayers; ++i) {
        const double h = (i == 0) ? config.first_layer_thickness : anchorThickness;
        currentZ += h;
        auto* anchor = addLayer(static_cast<std::size_t>(i), h, currentZ, 0.0);

        if (prev) { prev->upperLayer = anchor; anchor->lowerLayer = prev; }

        for (std::size_t ri = 0; ri < plate_->regionCount(); ++ri)
            anchor->addRegion(plate_->getRegion(ri));

        prev = anchor;
    }

    // Re-attach original layers with Z offset
    const double zOffset = currentZ;
    for (std::size_t i = 0; i < original.size(); ++i) {
        auto* layer = original[i];
        layer->setId(static_cast<std::size_t>(numAnchorLayers) + i);
        layer->setPrintZ(layer->printZ() + zOffset);
        layers_.push_back(layer);

        if (prev) { prev->upperLayer = layer; layer->lowerLayer = prev; }
        prev = layer;
    }

    if (!layers_.empty()) layers_.back()->upperLayer = nullptr;

    state.setDone(ObjectStep::Anchors);
}

// ==============================================================================
// PrintObject — Pipeline: Surface Detection  (delegates to SurfaceClassifier)
// ==============================================================================

void PrintObject::detectSurfaceTypes()
{
    if (state.isDone(ObjectStep::SurfaceDetection)) return;
    state.setStarted(ObjectStep::SurfaceDetection);

    surfaceClassifier_.classifyAll(layers_);

    state.setDone(ObjectStep::SurfaceDetection);
}

// ==============================================================================
// PrintObject — Pipeline: Infill Preparation
// ==============================================================================

void PrintObject::prepareInfill()
{
    if (state.isDone(ObjectStep::InfillPrep)) return;
    state.setStarted(ObjectStep::InfillPrep);

    if (!state.isDone(ObjectStep::Slicing))         slice();
    if (!state.isDone(ObjectStep::SurfaceDetection)) detectSurfaceTypes();

    for (auto* layer : layers_) {
        for (std::size_t ri = 0; ri < layer->regionCount(); ++ri) {
            auto* lr = layer->getRegion(ri);
            if (lr) lr->prepareFillSurfaces();
        }
    }

    state.setDone(ObjectStep::InfillPrep);
}

// ==============================================================================
// PrintObject — Pipeline: Support Material  (delegates to OverhangDetector
//                                             and SupportGenerator)
// ==============================================================================

bool PrintObject::hasSupportMaterial() const noexcept
{
    return config.support_material || config.anchors > 0.0;
}

void PrintObject::generateSupportMaterial()
{
    if (state.isDone(ObjectStep::SupportMaterial)) return;
    state.setStarted(ObjectStep::SupportMaterial);

    clearSupportLayers();

    if (!config.support_material || layers_.size() < 2) {
        state.setDone(ObjectStep::SupportMaterial);
        return;
    }

    std::cout << "  PrintObject: Detecting overhangs …\n";
    const auto overhangs = overhangDetector_.detect(
        layers_, config.support_material_angle);

    if (!overhangs.empty()) {
        std::cout << "  PrintObject: Generating support pillars …\n";
        supportGen_.generate(layers_, overhangs, config);
    }

    state.setDone(ObjectStep::SupportMaterial);
}

// ==============================================================================
// PrintObject — State Invalidation
// ==============================================================================

bool PrintObject::invalidateStep(ObjectStep step)
{
    bool inv = state.invalidate(step);

    switch (step) {
        case ObjectStep::Slicing:
            inv |= invalidateStep(ObjectStep::SurfaceDetection);
            inv |= invalidateStep(ObjectStep::SupportMaterial);
            break;
        case ObjectStep::SurfaceDetection:
            inv |= invalidateStep(ObjectStep::InfillPrep);
            break;
        case ObjectStep::InfillPrep:
            inv |= invalidateStep(ObjectStep::Infill);
            break;
        case ObjectStep::LayerGeneration:
            inv |= invalidateStep(ObjectStep::Slicing);
            break;
        default:
            break;
    }
    return inv;
}

bool PrintObject::invalidateAllSteps()
{
    return state.invalidateAll();
}

// ==============================================================================
// BuildPlate — Construction / Destruction
// ==============================================================================

BuildPlate::BuildPlate()  = default;

BuildPlate::~BuildPlate()
{
    clearUnifiedLayers();
    clearObjects();
    clearRegions();
}

// ==============================================================================
// BuildPlate — Configuration
// ==============================================================================

void BuildPlate::applyConfig(const SlmConfig& slmConfig)
{
    config = slmConfig;
    for (auto* obj    : objects_) obj->config    = slmConfig;
    for (auto* region : regions_) region->config = slmConfig;
    invalidateAllSteps();
}

// ==============================================================================
// BuildPlate — Model Management
// ==============================================================================

PrintObject* BuildPlate::addModel(const InternalModel& model)
{
    try {
        auto processor = std::make_unique<Geometry::MeshProcessor>();
        processor->loadMesh(model.path);

        if (!processor->hasValidMesh()) {
            std::cerr << "BuildPlate: Failed to load mesh: " << model.path << '\n';
            return nullptr;
        }

        auto* obj = new PrintObject(this, model, *processor);
        obj->config = config;

        if (regions_.empty()) addRegion();

        obj->regionVolumes[0].push_back(0);

        objects_.push_back(obj);
        meshProcessors_.push_back(std::move(processor));

        // Track the placement for build-plate preparation
        placements_.emplace_back(model);
        placements_.back().modelId = static_cast<uint32_t>(objects_.size() - 1);

        std::cout << "BuildPlate: Added model '" << model.path
                  << "' (object " << objects_.size() - 1 << ")\n";

        invalidateStep(BuildStep::Arrangement);
        prepared_ = false;
        return obj;

    } catch (const std::exception& e) {
        std::cerr << "BuildPlate: Error adding model: " << e.what() << '\n';
        return nullptr;
    }
}

std::size_t BuildPlate::addModels(const std::vector<InternalModel>& models)
{
    std::size_t count = 0;
    for (const auto& m : models) {
        if (addModel(m)) ++count;
    }
    return count;
}

void BuildPlate::clearObjects()
{
    for (auto* obj : objects_) {
        obj->invalidateAllSteps();
        delete obj;
    }
    objects_.clear();
    meshProcessors_.clear();
    placements_.clear();
    clearUnifiedLayers();
    clearRegions();
    prepared_ = false;
}

void BuildPlate::deleteObject(std::size_t idx)
{
    if (idx >= objects_.size()) return;
    objects_[idx]->invalidateAllSteps();
    delete objects_[idx];
    objects_.erase(objects_.begin() + static_cast<std::ptrdiff_t>(idx));

    if (idx < meshProcessors_.size()) {
        meshProcessors_.erase(
            meshProcessors_.begin() + static_cast<std::ptrdiff_t>(idx));
    }
    if (idx < placements_.size()) {
        placements_.erase(
            placements_.begin() + static_cast<std::ptrdiff_t>(idx));
    }
    clearUnifiedLayers();   // unified layers reference per-object regions
    prepared_ = false;
}

PrintObject* BuildPlate::getObject(std::size_t idx) noexcept
{
    return (idx < objects_.size()) ? objects_[idx] : nullptr;
}

const PrintObject* BuildPlate::getObject(std::size_t idx) const noexcept
{
    return (idx < objects_.size()) ? objects_[idx] : nullptr;
}

// ==============================================================================
// BuildPlate — Region Management
// ==============================================================================

PrintRegion* BuildPlate::addRegion()
{
    auto* r   = new PrintRegion(this);
    r->config = config;
    regions_.push_back(r);
    return r;
}

PrintRegion* BuildPlate::getRegion(std::size_t idx) noexcept
{
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

const PrintRegion* BuildPlate::getRegion(std::size_t idx) const noexcept
{
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

void BuildPlate::clearRegions()
{
    for (auto* r : regions_) delete r;
    regions_.clear();
}

// ==============================================================================
// BuildPlate — Unified Layer Stack Lifecycle
// ==============================================================================

void BuildPlate::clearUnifiedLayers()
{
    for (auto* l : unifiedLayers_) delete l;
    unifiedLayers_.clear();
}

// ==============================================================================
// BuildPlate — Bed Size  (propagate to preparator immediately)
// ==============================================================================

void BuildPlate::setBedSize(float width, float depth)
{
    bedWidth_ = width;
    bedDepth_ = depth;
    preparator_.setBedSize(width, depth);   // keep preparator in sync
}

// ==============================================================================
// BuildPlate — Build Plate Preparation
// ==============================================================================

void BuildPlate::prepareBuildPlate()
{
    if (objects_.empty()) {
        throw std::runtime_error("BuildPlate: No objects to prepare.");
    }

    reportProgress("Preparing build plate …", 0);

    preparator_.setBedSize(bedWidth_, bedDepth_);
    preparator_.setMinSpacing(config.duplicate_distance);
    preparator_.setProgressCallback(progressCb_);

    preparator_.prepare(placements_, meshProcessors_);

    // Sync PrintObject state from (possibly updated) placements and meshes
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        auto* obj  = objects_[i];
        auto& proc = meshProcessors_[i];
        auto& pl   = placements_[i];

        obj->placement.x     = pl.x;
        obj->placement.y     = pl.y;
        obj->placement.z     = pl.z;
        obj->placement.roll  = pl.roll;
        obj->placement.pitch = pl.pitch;
        obj->placement.yaw   = pl.yaw;

        if (proc && proc->hasValidMesh()) {
            const auto bb = proc->getBoundingBox();
            obj->sizeX = static_cast<double>(bb.sizeX());
            obj->sizeY = static_cast<double>(bb.sizeY());
            obj->sizeZ = static_cast<double>(bb.sizeZ());
        }
    }

    // Ensure we have exactly one region per object for per-model identity
    if (regions_.empty()) addRegion();
    while (regions_.size() < objects_.size()) addRegion();

    // Assign each object to its own region
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        objects_[i]->regionVolumes.clear();
        objects_[i]->regionVolumes[i].push_back(0);
    }

    prepared_ = true;
    state.setDone(BuildStep::Arrangement);

    reportProgress("Build plate preparation complete.", 40);
    std::cout << "BuildPlate: Prepared " << objects_.size()
              << " objects across " << regions_.size() << " regions.\n";

    const auto plateBB = boundingBox();
    std::cout << "  Plate bounding box: ["
              << plateBB.min.x << ", " << plateBB.min.y << ", " << plateBB.min.z
              << "] ? ["
              << plateBB.max.x << ", " << plateBB.max.y << ", " << plateBB.max.z
              << "]\n";
}

// ==============================================================================
// BuildPlate — Processing Pipeline
// ==============================================================================

void BuildPlate::process()
{
    if (!prepared_) {
        reportProgress("Auto-preparing build plate …", 0);
        prepareBuildPlate();
    }

    reportProgress("Starting build plate processing …", 0);

    // -------------------------------------------------------------------------
    // Unified slicing: slice ALL models at the same Z-heights so that every
    // output layer contains the scan paths of every model simultaneously.
    // This replaces the old per-object obj->slice() loop.
    // -------------------------------------------------------------------------
    reportProgress("Unified slicing of all models …", 10);
    sliceAllUnified();

    // Post-slice steps run on per-object layer stacks that sliceAllUnified()
    // populated in sync with the unified stack.
    reportProgress("Adding anchor layers …", 30);
    for (auto* obj : objects_) obj->addAnchorLayers();

    reportProgress("Detecting surface types …", 50);
    for (auto* obj : objects_) obj->detectSurfaceTypes();

    reportProgress("Preparing infill …", 70);
    for (auto* obj : objects_) obj->prepareInfill();

    reportProgress("Generating support material …", 85);
    for (auto* obj : objects_) obj->generateSupportMaterial();

    reportProgress("Build plate processing complete.", 100);
}

// ==============================================================================
// BuildPlate — Unified Slicing  (Task 1 — SLM-correct multi-model slicing)
// ==============================================================================
//
// Design (ported from Legacy Model pipeline, extended for SLM):
//
//   1. Find the tallest model ? derive a single shared Z-height sequence using
//      LayerHeightGenerator so every model is sliced at identical Z planes.
//
//   2. Allocate one BuildLayer per Z-height in unifiedLayers_.
//      Each BuildLayer gets ONE BuildLayerRegion PER MODEL (region index ==
//      object index) so per-model identity is preserved through region tags.
//
//   3. Slice every model's mesh at the shared sliceZ list.  The resulting
//      ExPolygons are written into each layer's region that corresponds to
//      that model.  Models that are shorter than the tallest simply produce
//      empty regions above their own height.
//
//   4. The per-object PrintObject::layers_ is also populated in lock-step so
//      that the legacy surface-detection and support-generation steps that
//      operate on per-object stacks still work correctly.
//
// ==============================================================================

void BuildPlate::sliceAllUnified()
{
    clearUnifiedLayers();
    if (objects_.empty()) return;

    // Ensure we have one region per object
    while (regions_.size() < objects_.size()) addRegion();

    // ------------------------------------------------------------------
    // Step 1 – Determine the tallest model ? unified Z-height sequence
    // ------------------------------------------------------------------
    double maxHeight = 0.0;
    for (const auto* obj : objects_) {
        if (obj->sizeZ > maxHeight) maxHeight = obj->sizeZ;
    }

    if (maxHeight <= 0.0) {
        std::cerr << "BuildPlate::sliceAllUnified: all models have zero height, "
                     "skipping slicing.\n";
        return;
    }

    // Use the first object's config (all objects share the plate config)
    const SlmConfig& cfg = objects_.front()->config;

    BP::LayerHeightGenerator layerGen;
    const std::vector<double> zHeights =
        layerGen.generate(cfg, maxHeight, cfg.first_layer_thickness);

    if (zHeights.empty()) {
        std::cerr << "BuildPlate::sliceAllUnified: LayerHeightGenerator "
                     "returned empty sequence.\n";
        return;
    }

    // Build the mid-plane Z list that the mesh slicer expects
    std::vector<float> sliceZs;
    sliceZs.reserve(zHeights.size());
    {
        double lo = 0.0;
        for (const double zTop : zHeights) {
            sliceZs.push_back(static_cast<float>(0.5 * (lo + zTop)));
            lo = zTop;
        }
    }

    std::cout << "BuildPlate::sliceAllUnified: " << objects_.size()
              << " models, " << zHeights.size()
              << " unified layers (max height = " << maxHeight << " mm).\n";

    // ------------------------------------------------------------------
    // Step 2 – Allocate unified BuildLayer stack, one region per model
    // ------------------------------------------------------------------
    unifiedLayers_.reserve(zHeights.size());

    // We use object[0] as the nominal owner of the unified layers; the
    // per-object stacks are populated separately below.
    PrintObject* nominalOwner = objects_[0];

    BuildLayer* prev = nullptr;
    double lo = 0.0;
    for (std::size_t li = 0; li < zHeights.size(); ++li) {
        const double hi     = zHeights[li];
        const double sliceZ = static_cast<double>(sliceZs[li]);

        auto* layer = new BuildLayer(li, nominalOwner, hi - lo, hi, sliceZ);

        // Add one BuildLayerRegion for every model so each model's
        // contours land in its own region within the shared layer.
        for (std::size_t oi = 0; oi < objects_.size(); ++oi) {
            layer->addRegion(regions_[oi]);
        }

        if (prev) { prev->upperLayer = layer; layer->lowerLayer = prev; }
        unifiedLayers_.push_back(layer);

        prev = layer;
        lo   = hi;
    }
    if (!unifiedLayers_.empty()) unifiedLayers_.back()->upperLayer = nullptr;

    // ------------------------------------------------------------------
    // Step 3 – Slice every model and populate regions in unified layers
    // ------------------------------------------------------------------
    for (std::size_t oi = 0; oi < objects_.size(); ++oi) {
        auto* obj  = objects_[oi];
        auto& proc = meshProcessors_[oi];

        // Clear any stale per-object layer stack first
        obj->clearLayers();

        if (!proc || !proc->hasValidMesh()) {
            std::cerr << "  Object " << oi << ": no valid mesh — skipped.\n";
            continue;
        }

        // Slice this model at the SHARED Z list
        const auto exPolysByLayer = proc->mesh_->slice(sliceZs);

        // Populate unified layers (region index == object index)
        // AND populate the per-object layer stack so surface-detection
        // and support-generation still have data to work with.
        lo = 0.0;
        for (std::size_t li = 0;
             li < zHeights.size() && li < unifiedLayers_.size();
             ++li)
        {
            const double hi     = zHeights[li];
            const double sliceZ = static_cast<double>(sliceZs[li]);

            // --- Unified layer: region[oi] receives this model's contours ---
            auto* unifiedLayer = unifiedLayers_[li];
            auto* unifiedRegion = unifiedLayer->getRegion(oi);

            // --- Per-object layer: one region, same geometry ----------------
            auto* perObjLayer = obj->addLayer(li, hi - lo, hi, sliceZ);
            perObjLayer->addRegion(regions_[oi]);

            auto* perObjRegion = perObjLayer->getRegion(0);

            // Wire up linked-list pointers for per-object stack
            if (li > 0) {
                auto* prevObjLayer = obj->getLayer(li - 1);
                if (prevObjLayer) {
                    prevObjLayer->upperLayer = perObjLayer;
                    perObjLayer->lowerLayer  = prevObjLayer;
                }
            }

            lo = hi;

            // Convert ExPolygons and write into both region targets
            if (li >= exPolysByLayer.size()) continue;

            for (const auto& exPoly : exPolysByLayer[li]) {
                ClassifiedSurface surf;
                surf.contour.reserve(exPoly.contour.size());

                for (const auto& pt : exPoly.contour)
                    surf.contour.emplace_back(pt.x, pt.y);

                for (const auto& hole : exPoly.holes) {
                    Clipper2Lib::Path64 hp;
                    hp.reserve(hole.size());
                    for (const auto& pt : hole) hp.emplace_back(pt.x, pt.y);
                    surf.holes.push_back(std::move(hp));
                }

                surf.type = SurfaceType::Internal;

                // Write to unified region (shared multi-model layer)
                if (unifiedRegion)
                    unifiedRegion->slices.push_back(surf);

                // Write to per-object region (for post-slice steps)
                if (perObjRegion)
                    perObjRegion->slices.push_back(std::move(surf));
            }

            // Merge slices for the per-object layer
            perObjLayer->makeSlices();
        }

        // Mark the per-object slicing step as done so downstream steps
        // (surface detection, infill, support) don't re-trigger a slice.
        obj->state.setStarted(ObjectStep::Slicing);
        obj->state.setDone(ObjectStep::Slicing);

        std::cout << "  Object " << oi << " ('" << obj->buildPlate()
                  << "'): sliced " << obj->layerCount() << " layers at "
                  << zHeights.size() << " unified Z heights.\n";
    }

    // Build mergedSlices for every unified layer (used by path planning)
    for (auto* layer : unifiedLayers_) {
        layer->makeSlices();
    }

    std::cout << "BuildPlate::sliceAllUnified: done — "
              << unifiedLayers_.size() << " unified layers created.\n";
}

// ==============================================================================
// BuildPlate — Validation
// ==============================================================================

void BuildPlate::validate() const
{
    if (objects_.empty())
        throw std::runtime_error("BuildPlate: No objects to process.");
    if (config.layer_thickness <= 0.0)
        throw std::runtime_error("BuildPlate: Invalid layer thickness.");
    if (config.hatch_spacing <= 0.0)
        throw std::runtime_error("BuildPlate: Invalid hatch spacing.");
}

// ==============================================================================
// BuildPlate — Export  (uses unified layer stack when available)
// ==============================================================================

std::vector<Marc::Layer> BuildPlate::exportLayers() const
{
    BP::LayerExporter exporter;

    // If unified slicing has been run, use the unified layer stack.
    // Every unified layer already contains all models' contours in
    // separate regions, so no interleaving occurs.
    if (!unifiedLayers_.empty()) {
        // Wrap unified layers in the format expected by LayerExporter
        std::vector<std::vector<const BuildLayer*>> singleStack;
        singleStack.emplace_back();
        singleStack.back().reserve(unifiedLayers_.size());
        for (const auto* l : unifiedLayers_)
            singleStack.back().push_back(l);
        return exporter.exportAll(singleStack);
    }

    // Fallback: legacy per-object export (all models sliced independently)
    std::vector<std::vector<const BuildLayer*>> objectLayers;
    objectLayers.reserve(objects_.size());
    for (const auto* obj : objects_) {
        std::vector<const BuildLayer*> objL;
        objL.reserve(obj->layerCount());
        for (std::size_t i = 0; i < obj->layerCount(); ++i)
            objL.push_back(obj->getLayer(i));
        objectLayers.push_back(std::move(objL));
    }
    return exporter.exportAll(objectLayers);
}

// ==============================================================================
// BuildPlate — Bounding Box
// ==============================================================================

Geometry::BBox3f BuildPlate::boundingBox() const
{
    Geometry::BBox3f bb;
    for (const auto* obj : objects_) {
        const auto& ob = obj->boundingBox();
        bb.merge(Geometry::Vertex3f(
            ob.min.x + static_cast<float>(obj->placement.x),
            ob.min.y + static_cast<float>(obj->placement.y),
            ob.min.z + static_cast<float>(obj->placement.z)));
        bb.merge(Geometry::Vertex3f(
            ob.max.x + static_cast<float>(obj->placement.x),
            ob.max.y + static_cast<float>(obj->placement.y),
            ob.max.z + static_cast<float>(obj->placement.z)));
    }
    return bb;
}

// ==============================================================================
// BuildPlate — State
// ==============================================================================

bool BuildPlate::invalidateStep(BuildStep step)
{
    return state.invalidate(step);
}

bool BuildPlate::invalidateAllSteps()
{
    bool inv = state.invalidateAll();
    for (auto* obj : objects_) inv |= obj->invalidateAllSteps();
    return inv;
}

// ==============================================================================
// BuildPlate — Progress
// ==============================================================================

void BuildPlate::reportProgress(const char* msg, int pct)
{
    if (progressCb_) progressCb_(msg, pct);
    std::cout << "  [" << pct << "%] " << msg << '\n';
}

// ==============================================================================
// BuildPlate — arrangeModels  (Legacy Geometry::arrange() cell-grid algorithm)
// ==============================================================================
//
// Ported faithfully from Legacy Slic3r Geometry::arrange():
//
//   1. Determine the largest model footprint ? uniform cell size
//      = (maxW + gap) × (maxD + gap).
//
//   2. Compute how many cells fit inside the bed (cellw × cellh).
//      Return false if total_parts > cellw*cellh.
//
//   3. For each candidate cell (i,j), compute a priority score equal to
//      squared-distance from the bed centre — cells closer to the centre
//      score lower and are filled first (nearest-to-centre packing).
//
//   4. Assign the sorted cells one per model.  Apply a final translation
//      so the occupied sub-grid is centred on the bed.
//
// This matches the behaviour of Legacy Model::arrange_objects().
// ==============================================================================

bool BuildPlate::arrangeModels(double spacing)
{
    if (objects_.empty()) return true;

    // Gap between models [mm] — enforce minimum 5 mm
    const double gap = std::max(5.0, (spacing > 0.0) ? spacing
                                                      : (double)config.duplicate_distance);

    const double bedW = static_cast<double>(bedWidth_);
    const double bedD = static_cast<double>(bedDepth_);

    const std::size_t total = objects_.size();

    // ------------------------------------------------------------------
    // 1. Collect per-model sizes and compute the uniform cell dimensions
    //    (largest footprint + gap on each side, as in Legacy arrange())
    // ------------------------------------------------------------------
    struct ModelInfo { std::size_t idx; double w; double d; };
    std::vector<ModelInfo> infos;
    infos.reserve(total);

    double maxW = 0.0, maxD = 0.0;
    for (std::size_t i = 0; i < total; ++i) {
        if (!meshProcessors_[i] || !meshProcessors_[i]->hasValidMesh()) {
            infos.push_back({i, 0.0, 0.0});
            continue;
        }
        const auto bb = meshProcessors_[i]->getBoundingBox();
        const double w = static_cast<double>(bb.sizeX());
        const double d = static_cast<double>(bb.sizeY());
        infos.push_back({i, w, d});
        if (w > maxW) maxW = w;
        if (d > maxD) maxD = d;
    }

    if (maxW <= 0.0 || maxD <= 0.0) return true;  // nothing to arrange

    // Uniform cell = largest model + gap (half on each side)
    const double cellW = maxW + gap;
    const double cellD = maxD + gap;

    // ------------------------------------------------------------------
    // 2. How many cells fit inside the bed?
    // ------------------------------------------------------------------
    const std::size_t cellCols = static_cast<std::size_t>(
        std::floor((bedW + gap) / cellW));
    const std::size_t cellRows = static_cast<std::size_t>(
        std::floor((bedD + gap) / cellD));

    if (cellCols == 0 || cellRows == 0 || total > cellCols * cellRows) {
        std::cerr << "BuildPlate::arrangeModels: " << total
                  << " models do not fit on a " << bedW << "x" << bedD
                  << " mm bed with " << gap << " mm gap.\n";
        return false;
    }

    // ------------------------------------------------------------------
    // 3. Build the full cell grid and sort by distance from bed centre
    // ------------------------------------------------------------------
    struct Cell {
        std::size_t col, row;
        double      cx, cy;   // cell centre in bed coords [mm]
        double      dist;     // squared distance from bed centre
    };

    // Total cells bounding box (all cells packed flush from 0,0)
    const double totalCellsW = cellCols * cellW;
    const double totalCellsD = cellRows * cellD;

    // Offset so the cell grid is centred on the bed
    const double gridOffX = (bedW - totalCellsW) / 2.0;
    const double gridOffY = (bedD - totalCellsD) / 2.0;

    const double bedCX = bedW / 2.0;
    const double bedCY = bedD / 2.0;

    std::vector<Cell> cells;
    cells.reserve(cellCols * cellRows);

    for (std::size_t ci = 0; ci < cellCols; ++ci) {
        for (std::size_t ri = 0; ri < cellRows; ++ri) {
            Cell c;
            c.col = ci;
            c.row = ri;
            // Centre of cell in bed coordinates
            c.cx  = gridOffX + ci * cellW + cellW / 2.0;
            c.cy  = gridOffY + ri * cellD + cellD / 2.0;
            const double dx = c.cx - bedCX;
            const double dy = c.cy - bedCY;
            // Legacy tiebreaker: slightly favour columns closer to left
            c.dist = dx*dx + dy*dy - std::abs((double)cellCols/2.0 - (ci + 0.5));
            cells.push_back(c);
        }
    }

    // Sort ascending by distance (nearest centre first)
    std::sort(cells.begin(), cells.end(),
              [](const Cell& a, const Cell& b){ return a.dist < b.dist; });

    // ------------------------------------------------------------------
    // 4. Assign the first `total` cells to models and apply translations
    // ------------------------------------------------------------------
    for (std::size_t mi = 0; mi < total; ++mi) {
        const std::size_t oi   = infos[mi].idx;
        const Cell&       cell = cells[mi];
        auto&             proc = meshProcessors_[oi];

        if (!proc || !proc->hasValidMesh()) continue;

        const auto   bb    = proc->getBoundingBox();
        const double mW    = static_cast<double>(bb.sizeX());
        const double mD    = static_cast<double>(bb.sizeY());

        // Centre model within its cell
        const double destX = cell.cx - mW / 2.0;
        const double destY = cell.cy - mD / 2.0;

        const double dx = destX - static_cast<double>(bb.min.x);
        const double dy = destY - static_cast<double>(bb.min.y);

        proc->mesh_->translate(static_cast<float>(dx),
                               static_cast<float>(dy), 0.0f);
        proc->bbox_.reset();

        objects_[oi]->placement.x += dx;
        objects_[oi]->placement.y += dy;
        if (oi < placements_.size()) {
            placements_[oi].x += dx;
            placements_[oi].y += dy;
        }

        std::cout << "  Model " << oi << ": cell [" << cell.col << "," << cell.row
                  << "] ? (" << destX << ", " << destY << ") mm"
                  << "  [size " << mW << " × " << mD << " mm]\n";
    }

    std::cout << "BuildPlate::arrangeModels: " << total
              << " models in a " << cellCols << "x" << cellRows
              << " grid, gap=" << gap << " mm, bed="
              << bedW << "x" << bedD << " mm.\n";

    invalidateStep(BuildStep::Arrangement);
    clearUnifiedLayers();
    return true;
}

// ==============================================================================
// BuildPlate — validateNoOverlap / validateFitsInBed  (thin wrappers)
// ==============================================================================

void BuildPlate::validateNoOverlap() const
{
    preparator_.validateNoOverlap(meshProcessors_,
                                  static_cast<float>(config.duplicate_distance));
}

void BuildPlate::validateFitsInBed() const
{
    preparator_.validateFitsInBed(meshProcessors_);
}

// ==============================================================================
// BuildPlate — applyAllPlacements / bboxOverlapXY
// ==============================================================================

void BuildPlate::applyAllPlacements()
{
    preparator_.applyAllPlacements(placements_, meshProcessors_);
}

bool BuildPlate::bboxOverlapXY(const Geometry::BBox3f& a,
                                const Geometry::BBox3f& b,
                                float gap) noexcept
{
    return BP::BuildPlatePreparator::bboxOverlapXY(a, b, gap);
}

} // namespace MarcSLM
