// ==============================================================================
// MarcSLM - Build Plate Preparation Implementation
// ==============================================================================
// Ported from Legacy Slic3r Print.cpp + PrintObject.cpp
// Industrial-quality build plate management for SLM process
// ==============================================================================

#include "MarcSLM/Core/BuildPlate.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace MarcSLM {

// ==============================================================================
// StepState explicit template instantiations
// ==============================================================================

template class StepState<BuildStep>;
template class StepState<ObjectStep>;

// ==============================================================================
// PrintRegion
// ==============================================================================

PrintRegion::PrintRegion(BuildPlate* plate) : plate_(plate) {}

// ==============================================================================
// BuildLayerRegion
// ==============================================================================

BuildLayerRegion::BuildLayerRegion(BuildLayer* layer, PrintRegion* region)
    : layer_(layer), region_(region) {}

void BuildLayerRegion::prepareFillSurfaces() {
    // Copy slices to fill surfaces if fill surfaces are empty.
    // In the SLM process, all internal regions are treated as solid fill.
    if (fillSurfaces.empty() && !slices.empty()) {
        fillSurfaces = slices;
    }
}

// ==============================================================================
// BuildLayer
// ==============================================================================

BuildLayer::BuildLayer(size_t id, PrintObject* object,
                       double height, double printZ, double sliceZ)
    : id_(id), object_(object), height_(height),
      printZ_(printZ), sliceZ_(sliceZ) {}

BuildLayer::~BuildLayer() {
    for (auto* r : regions_) {
        delete r;
    }
    regions_.clear();
}

BuildLayerRegion* BuildLayer::addRegion(PrintRegion* region) {
    auto* lr = new BuildLayerRegion(this, region);
    regions_.push_back(lr);
    return lr;
}

BuildLayerRegion* BuildLayer::getRegion(size_t idx) {
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

const BuildLayerRegion* BuildLayer::getRegion(size_t idx) const {
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

void BuildLayer::detectSurfaceTypes() {
    for (auto* layerRegion : regions_) {
        if (layerRegion->slices.empty()) continue;

        // Get upper and lower layer slices for comparison
        Clipper2Lib::Paths64 upperPaths;
        Clipper2Lib::Paths64 lowerPaths;

        if (upperLayer) {
            for (const auto* ulr : upperLayer->regions()) {
                for (const auto& surf : ulr->slices) {
                    if (surf.isValid()) {
                        upperPaths.push_back(surf.contour);
                    }
                }
            }
        }
        if (lowerLayer) {
            for (const auto* llr : lowerLayer->regions()) {
                for (const auto& surf : llr->slices) {
                    if (surf.isValid()) {
                        lowerPaths.push_back(surf.contour);
                    }
                }
            }
        }

        // Classify each surface
        for (auto& surf : layerRegion->slices) {
            if (!surf.isValid()) continue;

            Clipper2Lib::Paths64 currentPath = {surf.contour};

            // Check if this surface is exposed on top (no upper layer covering it)
            if (upperPaths.empty()) {
                surf.type = SurfaceType::Top;
            } else {
                Clipper2Lib::Paths64 uncoveredTop =
                    Clipper2Lib::BooleanOp(Clipper2Lib::ClipType::Difference,
                        Clipper2Lib::FillRule::NonZero,
                        currentPath, upperPaths);
                if (!uncoveredTop.empty()) {
                    // Part of this surface is exposed on top
                    surf.type = SurfaceType::Top;
                }
            }

            // Check if this surface is exposed on bottom (no lower layer supporting it)
            if (lowerPaths.empty()) {
                surf.type = SurfaceType::Bottom;
            } else {
                Clipper2Lib::Paths64 unsupportedBottom =
                    Clipper2Lib::BooleanOp(Clipper2Lib::ClipType::Difference,
                        Clipper2Lib::FillRule::NonZero,
                        currentPath, lowerPaths);
                if (!unsupportedBottom.empty() && surf.type != SurfaceType::Top) {
                    surf.type = SurfaceType::Bottom;
                }
            }

            // If neither top nor bottom, it's internal
            if (surf.type != SurfaceType::Top && surf.type != SurfaceType::Bottom) {
                surf.type = SurfaceType::Internal;
            }
        }
    }
}

void BuildLayer::makeSlices() {
    mergedSlices.clear();

    Clipper2Lib::Paths64 allPaths;
    for (const auto* lr : regions_) {
        for (const auto& surf : lr->slices) {
            if (surf.isValid()) {
                allPaths.push_back(surf.contour);
            }
        }
    }

    if (allPaths.empty()) return;

    // Union all region slices
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(allPaths);
    clipper.Execute(Clipper2Lib::ClipType::Union,
                    Clipper2Lib::FillRule::NonZero,
                    mergedSlices);
}

// ==============================================================================
// PrintObject: Construction
// ==============================================================================

PrintObject::PrintObject(BuildPlate* plate, const InternalModel& modelDesc,
                         Geometry::MeshProcessor& meshProcessor)
    : plate_(plate)
    , meshProcessor_(&meshProcessor)
    , modelDesc_(modelDesc) {
    // Set placement from model descriptor
    placement.x = modelDesc.xpos;
    placement.y = modelDesc.ypos;
    placement.z = modelDesc.zpos;
    placement.roll = modelDesc.roll;
    placement.pitch = modelDesc.pitch;
    placement.yaw = modelDesc.yaw;

    // Get bounding box from loaded mesh
    if (meshProcessor_->hasValidMesh()) {
        bbox_ = meshProcessor_->getBoundingBox();
        sizeX = static_cast<double>(bbox_.sizeX());
        sizeY = static_cast<double>(bbox_.sizeY());
        sizeZ = static_cast<double>(bbox_.sizeZ());
    }
}

PrintObject::~PrintObject() {
    clearLayers();
    clearSupportLayers();
}

BuildLayer* PrintObject::getLayer(size_t idx) {
    return (idx < layers_.size()) ? layers_[idx] : nullptr;
}

const BuildLayer* PrintObject::getLayer(size_t idx) const {
    return (idx < layers_.size()) ? layers_[idx] : nullptr;
}

BuildLayer* PrintObject::addLayer(size_t id, double height,
                                  double printZ, double sliceZ) {
    auto* layer = new BuildLayer(id, this, height, printZ, sliceZ);
    layers_.push_back(layer);
    return layer;
}

void PrintObject::deleteLayer(size_t idx) {
    if (idx >= layers_.size()) return;
    delete layers_[idx];
    layers_.erase(layers_.begin() + static_cast<ptrdiff_t>(idx));
}

void PrintObject::clearLayers() {
    for (auto* l : layers_) delete l;
    layers_.clear();
}

void PrintObject::clearSupportLayers() {
    for (auto* l : supportLayers_) delete l;
    supportLayers_.clear();
}

// ==============================================================================
// PrintObject: Layer Height Generation
// (Ported from Legacy PrintObject::generate_object_layers)
// ==============================================================================

std::vector<double> PrintObject::generateObjectLayers(double firstLayerHeight) {
    std::vector<double> result;

    double layerHeight = config.layer_thickness;

    // Enforce SLM layer height constraints
    if (config.z_steps_per_mm > 0) {
        double minDz = 1.0 / config.z_steps_per_mm;
        layerHeight = std::round(layerHeight / minDz) * minDz;
        if (layerHeight <= 0.0) layerHeight = config.layer_thickness;
    }

    // Respect first layer height
    if (firstLayerHeight > 0.0) {
        result.push_back(firstLayerHeight);
    }

    double printZ = firstLayerHeight;

    // Generate uniform layers (SLM always uses uniform layer height)
    while ((printZ + 1e-6) < sizeZ) {
        printZ += layerHeight;
        result.push_back(printZ);
    }

    // Adjust last layer to match object height precisely
    if (result.size() > 1 && config.z_steps_per_mm > 0) {
        double diff = result.back() - sizeZ;
        size_t lastIdx = result.size() - 1;
        double newH = result[lastIdx] - result[lastIdx - 1];

        if (diff < 0) {
            // Need to thicken last layer
            newH = std::min(layerHeight * 1.5, newH - diff);
        } else {
            // Need to thin last layer
            newH = std::max(layerHeight * 0.5, newH - diff);
        }
        result[lastIdx] = result[lastIdx - 1] + newH;
    }

    // Apply z-gradation if configured
    if (config.z_steps_per_mm > 0) {
        double gradation = 1.0 / config.z_steps_per_mm;
        double lastZ = 0.0;
        for (auto& z : result) {
            double h = z - lastZ;
            double remainder = std::fmod(h, gradation);
            if (remainder > gradation / 2.0) {
                h += (gradation - remainder);
            } else {
                h -= remainder;
            }
            z = lastZ + h;
            lastZ = z;
        }
    }

    return result;
}

// ==============================================================================
// PrintObject: Slicing
// (Ported from Legacy PrintObject::_slice)
// ==============================================================================

void PrintObject::slice() {
    if (state.isDone(ObjectStep::Slicing)) return;
    state.setStarted(ObjectStep::Slicing);

    std::cout << "  PrintObject: Slicing..." << std::endl;

    sliceInternal();

    // Remove empty layers from bottom
    while (!layers_.empty()) {
        bool hasContent = false;
        for (size_t ri = 0; ri < layers_[0]->regionCount(); ++ri) {
            if (layers_[0]->getRegion(ri) &&
                !layers_[0]->getRegion(ri)->slices.empty()) {
                hasContent = true;
                break;
            }
        }
        if (hasContent) break;
        deleteLayer(0);
        for (size_t i = 0; i < layers_.size(); ++i) {
            layers_[i]->setId(i);
        }
    }

    // Remove empty layers from top
    while (!layers_.empty()) {
        const auto* lastLayer = layers_.back();
        bool hasContent = false;
        for (size_t ri = 0; ri < lastLayer->regionCount(); ++ri) {
            if (lastLayer->getRegion(ri) &&
                !lastLayer->getRegion(ri)->slices.empty()) {
                hasContent = true;
                break;
            }
        }
        if (hasContent) break;
        deleteLayer(layers_.size() - 1);
    }

    if (layers_.empty()) {
        std::cerr << "  PrintObject: No layers generated after slicing!" << std::endl;
    } else {
        std::cout << "  PrintObject: " << layers_.size()
                  << " layers after slicing" << std::endl;
    }

    state.setDone(ObjectStep::Slicing);
}

void PrintObject::sliceInternal() {
    double firstLayerHeight = config.first_layer_thickness;
    double raftHeight = 0.0;

    // Generate layer heights
    std::vector<double> objectLayers = generateObjectLayers(firstLayerHeight);

    // Build slice Z-heights
    std::vector<float> sliceZs;
    clearLayers();
    sliceZs.reserve(objectLayers.size());

    BuildLayer* prev = nullptr;
    double lo = raftHeight;
    double hi = lo;
    size_t id = 0;

    for (size_t i = 0; i < objectLayers.size(); ++i) {
        lo = hi;
        hi = objectLayers[i] + raftHeight;
        double sliceZ = 0.5 * (lo + hi) - raftHeight;

        auto* layer = addLayer(id++, hi - lo, hi, sliceZ);
        sliceZs.push_back(static_cast<float>(sliceZ));

        if (prev) {
            prev->upperLayer = layer;
            layer->lowerLayer = prev;
        }

        // Add regions to layer
        for (size_t ri = 0; ri < plate_->regionCount(); ++ri) {
            layer->addRegion(plate_->getRegion(ri));
        }

        prev = layer;
    }

    // Perform the actual mesh slicing
    if (!meshProcessor_->hasValidMesh()) {
        std::cerr << "  PrintObject: No valid mesh for slicing!" << std::endl;
        return;
    }

    // Slice all Z-heights at once using TriMesh slicer
    auto exPolygonsByLayer = meshProcessor_->mesh_->slice(sliceZs);

    // Distribute ExPolygons to layer regions
    for (size_t layerIdx = 0; layerIdx < exPolygonsByLayer.size(); ++layerIdx) {
        if (layerIdx >= layers_.size()) break;

        auto* layer = layers_[layerIdx];
        if (layer->regionCount() == 0) continue;

        auto* regionLayer = layer->getRegion(0);
        if (!regionLayer) continue;

        const auto& exPolygons = exPolygonsByLayer[layerIdx];
        for (const auto& exPoly : exPolygons) {
            ClassifiedSurface surf;

            // Convert contour from TriMesh integer coordinates to Clipper2 coordinates
            surf.contour.reserve(exPoly.contour.size());
            for (const auto& pt : exPoly.contour) {
                surf.contour.emplace_back(pt.x, pt.y);
            }

            // Convert holes
            for (const auto& hole : exPoly.holes) {
                Clipper2Lib::Path64 holePath;
                holePath.reserve(hole.size());
                for (const auto& pt : hole) {
                    holePath.emplace_back(pt.x, pt.y);
                }
                surf.holes.push_back(std::move(holePath));
            }

            surf.type = SurfaceType::Internal;
            regionLayer->slices.push_back(std::move(surf));
        }
    }

    // Make merged slices for each layer
    for (auto* layer : layers_) {
        layer->makeSlices();
    }
}

std::vector<Geometry::ExPolygons2i> PrintObject::sliceRegion(
    size_t /*regionId*/, const std::vector<float>& zHeights) {
    if (!meshProcessor_->hasValidMesh()) return {};
    return meshProcessor_->mesh_->slice(zHeights);
}

// ==============================================================================
// PrintObject: Anchor Layers
// (Ported from Legacy PrintObject::add_anchor_layers)
// ==============================================================================

void PrintObject::addAnchorLayers() {
    if (state.isDone(ObjectStep::Anchors)) return;
    state.setStarted(ObjectStep::Anchors);

    double anchorThickness = config.anchors_layer_thickness;
    double totalAnchorHeight = config.anchors;
    double firstLayerHeight = config.first_layer_thickness;

    if (totalAnchorHeight <= 0.0) {
        state.setDone(ObjectStep::Anchors);
        return;
    }

    int numAnchorLayers = static_cast<int>(totalAnchorHeight / anchorThickness);
    if (numAnchorLayers <= 0) numAnchorLayers = 1;

    std::cout << "  PrintObject: Adding " << numAnchorLayers
              << " anchor layers" << std::endl;

    // Save original layers
    std::vector<BuildLayer*> originalLayers;
    originalLayers.reserve(layers_.size());
    for (auto* l : layers_) {
        originalLayers.push_back(l);
    }
    layers_.clear();
    layers_.reserve(static_cast<size_t>(numAnchorLayers) + originalLayers.size());

    // Create anchor layers
    BuildLayer* prevLayer = nullptr;
    double currentHeight = 0.0;

    for (int i = 0; i < numAnchorLayers; ++i) {
        double layerH = (i == 0) ? firstLayerHeight : anchorThickness;
        currentHeight += layerH;

        auto* anchorLayer = addLayer(
            static_cast<size_t>(i), layerH, currentHeight, 0.0);

        if (prevLayer) {
            prevLayer->upperLayer = anchorLayer;
            anchorLayer->lowerLayer = prevLayer;
        }

        // Add regions to anchor layer
        for (size_t ri = 0; ri < plate_->regionCount(); ++ri) {
            anchorLayer->addRegion(plate_->getRegion(ri));
        }

        prevLayer = anchorLayer;
    }

    // Re-attach original layers with offset Z
    double anchorZOffset = currentHeight;
    for (size_t i = 0; i < originalLayers.size(); ++i) {
        auto* layer = originalLayers[i];
        layer->setId(static_cast<size_t>(numAnchorLayers) + i);
        layer->setPrintZ(layer->printZ() + anchorZOffset);
        layers_.push_back(layer);

        if (prevLayer) {
            prevLayer->upperLayer = layer;
            layer->lowerLayer = prevLayer;
        }
        prevLayer = layer;
    }

    if (!layers_.empty()) {
        layers_.back()->upperLayer = nullptr;
    }

    state.setDone(ObjectStep::Anchors);
}

// ==============================================================================
// PrintObject: Surface Detection
// ==============================================================================

void PrintObject::detectSurfaceTypes() {
    if (state.isDone(ObjectStep::SurfaceDetection)) return;
    state.setStarted(ObjectStep::SurfaceDetection);

    for (auto* layer : layers_) {
        layer->detectSurfaceTypes();
    }

    state.setDone(ObjectStep::SurfaceDetection);
}

// ==============================================================================
// PrintObject: Infill Preparation
// ==============================================================================

void PrintObject::prepareInfill() {
    if (state.isDone(ObjectStep::InfillPrep)) return;
    state.setStarted(ObjectStep::InfillPrep);

    // Prerequisite: slicing and surface detection must be done
    if (!state.isDone(ObjectStep::Slicing)) {
        slice();
    }
    if (!state.isDone(ObjectStep::SurfaceDetection)) {
        detectSurfaceTypes();
    }

    for (auto* layer : layers_) {
        for (size_t ri = 0; ri < layer->regionCount(); ++ri) {
            auto* layerRegion = layer->getRegion(ri);
            if (layerRegion) {
                layerRegion->prepareFillSurfaces();
            }
        }
    }

    state.setDone(ObjectStep::InfillPrep);
}

// ==============================================================================
// PrintObject: Support Material
// (Ported from Legacy PrintObject::generate_support_structure)
// ==============================================================================

bool PrintObject::hasSupportMaterial() const {
    return config.support_material || config.anchors > 0;
}

void PrintObject::generateSupportMaterial() {
    if (state.isDone(ObjectStep::SupportMaterial)) return;
    state.setStarted(ObjectStep::SupportMaterial);

    clearSupportLayers();

    if (!config.support_material || layers_.size() < 2) {
        state.setDone(ObjectStep::SupportMaterial);
        return;
    }

    std::cout << "  PrintObject: Generating support material..." << std::endl;

    double supportAngle = config.support_material_angle;
    double pillarSpacing = config.support_material_pillar_spacing;
    double pillarSize = config.support_material_pillar_size;

    // Detect overhangs
    std::map<size_t, std::map<size_t, Clipper2Lib::Paths64>> overhangAreas;
    detectOverhangs(overhangAreas, supportAngle);

    // Generate pillar positions
    std::map<size_t, std::vector<Clipper2Lib::Point64>> pillarPositions;
    generatePillarPositions(overhangAreas, pillarPositions, pillarSpacing);

    // Create support geometry
    createSupportPillars(pillarPositions, pillarSize);

    state.setDone(ObjectStep::SupportMaterial);
}

void PrintObject::detectOverhangs(
    std::map<size_t, std::map<size_t, Clipper2Lib::Paths64>>& overhangAreas,
    double supportAngle) {

    const double tanAngle = std::tan(supportAngle * M_PI / 180.0);

    for (size_t layerIdx = 1; layerIdx < layers_.size(); ++layerIdx) {
        auto* layer = layers_[layerIdx];
        auto* lowerLayer = layers_[layerIdx - 1];

        for (size_t ri = 0; ri < layer->regionCount(); ++ri) {
            auto* layerRegion = layer->getRegion(ri);
            auto* lowerRegion = lowerLayer->getRegion(ri);
            if (!layerRegion || !lowerRegion) continue;
            if (layerRegion->slices.empty()) continue;

            double maxSupportDist = layer->height() / tanAngle;
            int64_t scaledDist = static_cast<int64_t>(
                maxSupportDist / Geometry::MESH_SCALING_FACTOR);

            // Get current layer paths
            Clipper2Lib::Paths64 currentPaths;
            for (const auto& surf : layerRegion->slices) {
                if (surf.isValid()) currentPaths.push_back(surf.contour);
            }

            // Get lower layer paths and expand them
            Clipper2Lib::Paths64 lowerPaths;
            for (const auto& surf : lowerRegion->slices) {
                if (surf.isValid()) lowerPaths.push_back(surf.contour);
            }

            Clipper2Lib::Paths64 supportedArea =
                Clipper2Lib::InflatePaths(lowerPaths, static_cast<double>(scaledDist),
                    Clipper2Lib::JoinType::Miter, Clipper2Lib::EndType::Polygon);

            // Unsupported areas = current - expanded lower
            Clipper2Lib::Paths64 unsupported =
                Clipper2Lib::BooleanOp(Clipper2Lib::ClipType::Difference,
                    Clipper2Lib::FillRule::NonZero,
                    currentPaths, supportedArea);

            if (!unsupported.empty()) {
                overhangAreas[ri][layerIdx] = unsupported;
            }
        }
    }
}

void PrintObject::generatePillarPositions(
    const std::map<size_t, std::map<size_t, Clipper2Lib::Paths64>>& overhangAreas,
    std::map<size_t, std::vector<Clipper2Lib::Point64>>& pillarPositions,
    double pillarSpacing) {

    int64_t scaledSpacing = static_cast<int64_t>(
        pillarSpacing / Geometry::MESH_SCALING_FACTOR);

    for (const auto& [regionId, layerOverhangs] : overhangAreas) {
        // Collect all overhang paths for this region
        Clipper2Lib::Paths64 allOverhangs;
        for (const auto& [layerId, paths] : layerOverhangs) {
            allOverhangs.insert(allOverhangs.end(), paths.begin(), paths.end());
        }

        // Union all overhangs
        Clipper2Lib::Clipper64 clipper;
        clipper.AddSubject(allOverhangs);
        Clipper2Lib::Paths64 unified;
        clipper.Execute(Clipper2Lib::ClipType::Union,
                        Clipper2Lib::FillRule::NonZero, unified);

        // Generate grid points within overhang areas
        std::vector<Clipper2Lib::Point64> regionPillars;
        for (const auto& path : unified) {
            // Compute bounding box
            int64_t minX = std::numeric_limits<int64_t>::max();
            int64_t maxX = std::numeric_limits<int64_t>::min();
            int64_t minY = std::numeric_limits<int64_t>::max();
            int64_t maxY = std::numeric_limits<int64_t>::min();
            for (const auto& pt : path) {
                minX = std::min(minX, pt.x);
                maxX = std::max(maxX, pt.x);
                minY = std::min(minY, pt.y);
                maxY = std::max(maxY, pt.y);
            }

            // Point-in-polygon grid sampling
            for (int64_t y = minY; y < maxY; y += scaledSpacing) {
                for (int64_t x = minX; x < maxX; x += scaledSpacing) {
                    Clipper2Lib::Point64 testPt(x, y);
                    if (Clipper2Lib::PointInPolygon(testPt, path) !=
                        Clipper2Lib::PointInPolygonResult::IsOutside) {
                        regionPillars.push_back(testPt);
                    }
                }
            }
        }

        if (!regionPillars.empty()) {
            pillarPositions[regionId] = std::move(regionPillars);
        }
    }
}

void PrintObject::createSupportPillars(
    const std::map<size_t, std::vector<Clipper2Lib::Point64>>& pillarPositions,
    double pillarSize) {

    std::map<size_t, std::vector<SupportPillar>> pillarsByRegion;

    for (const auto& [regionId, points] : pillarPositions) {
        std::vector<SupportPillar> regionPillars;

        for (const auto& pt : points) {
            SupportPillar pillar;
            pillar.x = static_cast<double>(pt.x) * Geometry::MESH_SCALING_FACTOR;
            pillar.y = static_cast<double>(pt.y) * Geometry::MESH_SCALING_FACTOR;
            pillar.radiusBase = pillarSize / 2.0;
            pillar.radiusTop = pillar.radiusBase * 0.8;
            pillar.topLayer = 0;
            pillar.bottomLayer = 0;

            // Determine pillar extent through layers
            bool objectHit = false;
            for (int i = static_cast<int>(layers_.size()) - 1; i >= 0; --i) {
                if (regionId >= layers_[static_cast<size_t>(i)]->regionCount())
                    continue;

                auto* layerRegion = layers_[static_cast<size_t>(i)]->getRegion(regionId);
                if (!layerRegion) continue;

                bool pointInside = false;
                for (const auto& surf : layerRegion->slices) {
                    if (Clipper2Lib::PointInPolygon(pt, surf.contour) !=
                        Clipper2Lib::PointInPolygonResult::IsOutside) {
                        pointInside = true;
                        break;
                    }
                }

                if (!pointInside) {
                    if (pillar.topLayer > 0) {
                        pillar.bottomLayer = static_cast<size_t>(i);
                    } else if (objectHit) {
                        pillar.topLayer = static_cast<size_t>(i);
                    }
                } else {
                    if (pillar.topLayer > 0) {
                        regionPillars.push_back(pillar);
                        pillar.topLayer = 0;
                        pillar.bottomLayer = 0;
                    }
                    objectHit = true;
                }
            }

            if (pillar.topLayer > 0) {
                regionPillars.push_back(pillar);
            }
        }

        if (!regionPillars.empty()) {
            pillarsByRegion[regionId] = std::move(regionPillars);
        }
    }

    generateSupportGeometry(pillarsByRegion, pillarSize);
}

void PrintObject::generateSupportGeometry(
    const std::map<size_t, std::vector<SupportPillar>>& pillarsByRegion,
    double pillarBaseSize) {

    for (size_t layerIdx = 0; layerIdx < layers_.size(); ++layerIdx) {
        auto* layer = layers_[layerIdx];

        for (const auto& [regionId, pillars] : pillarsByRegion) {
            if (regionId >= layer->regionCount()) continue;

            auto* layerRegion = layer->getRegion(regionId);
            if (!layerRegion) continue;

            double baseRadius = pillarBaseSize / 2.0;
            double topRadius = baseRadius * 0.2;

            for (const auto& pillar : pillars) {
                if (layerIdx < pillar.bottomLayer || layerIdx > pillar.topLayer)
                    continue;

                // Calculate tapered radius
                double heightPos = (pillar.topLayer > pillar.bottomLayer)
                    ? static_cast<double>(layerIdx - pillar.bottomLayer) /
                      static_cast<double>(pillar.topLayer - pillar.bottomLayer)
                    : 0.5;

                double radius;
                if (heightPos < 0.2) {
                    radius = baseRadius;
                } else if (heightPos > 0.8) {
                    radius = topRadius;
                } else {
                    radius = baseRadius - (baseRadius - topRadius) *
                             ((heightPos - 0.2) / 0.6);
                }

                // Create circular pillar polygon
                int64_t scaledRadius = static_cast<int64_t>(
                    radius / Geometry::MESH_SCALING_FACTOR);
                int64_t cx = static_cast<int64_t>(
                    pillar.x / Geometry::MESH_SCALING_FACTOR);
                int64_t cy = static_cast<int64_t>(
                    pillar.y / Geometry::MESH_SCALING_FACTOR);

                ClassifiedSurface supportSurf;
                int sides = std::max(8, static_cast<int>(radius / 0.1) * 2 + 8);
                supportSurf.contour.reserve(static_cast<size_t>(sides));

                for (int s = 0; s < sides; ++s) {
                    double angle = 2.0 * M_PI * s / sides;
                    supportSurf.contour.emplace_back(
                        cx + static_cast<int64_t>(scaledRadius * std::cos(angle)),
                        cy + static_cast<int64_t>(scaledRadius * std::sin(angle)));
                }

                supportSurf.type = SurfaceType::Support;
                layerRegion->supportSurfaces.push_back(std::move(supportSurf));
            }
        }
    }
}

bool PrintObject::invalidateStep(ObjectStep step) {
    bool invalidated = state.invalidate(step);

    // Propagate to dependent steps
    switch (step) {
        case ObjectStep::Slicing:
            invalidated |= invalidateStep(ObjectStep::SurfaceDetection);
            invalidated |= invalidateStep(ObjectStep::SupportMaterial);
            break;
        case ObjectStep::SurfaceDetection:
            invalidated |= invalidateStep(ObjectStep::InfillPrep);
            break;
        case ObjectStep::InfillPrep:
            invalidated |= invalidateStep(ObjectStep::Infill);
            break;
        case ObjectStep::LayerGeneration:
            invalidated |= invalidateStep(ObjectStep::Slicing);
            break;
        default:
            break;
    }
    return invalidated;
}

bool PrintObject::invalidateAllSteps() {
    return state.invalidateAll();
}

// ==============================================================================
// BuildPlate: Construction
// ==============================================================================

BuildPlate::BuildPlate() = default;

BuildPlate::~BuildPlate() {
    clearObjects();
    clearRegions();
}

// ==============================================================================
// BuildPlate: Configuration
// ==============================================================================

void BuildPlate::applyConfig(const SlmConfig& slmConfig) {
    config = slmConfig;

    for (auto* obj : objects_) {
        obj->config = slmConfig;
    }
    for (auto* region : regions_) {
        region->config = slmConfig;
    }

    invalidateAllSteps();
}

// ==============================================================================
// BuildPlate: Model Management
// ==============================================================================

PrintObject* BuildPlate::addModel(const InternalModel& model) {
    try {
        // Create mesh processor for this model
        auto processor = std::make_unique<Geometry::MeshProcessor>();
        processor->loadMesh(model.path);

        if (!processor->hasValidMesh()) {
            std::cerr << "BuildPlate: Failed to load mesh: "
                      << model.path << std::endl;
            return nullptr;
        }

        // Create print object
        auto* obj = new PrintObject(this, model, *processor);
        obj->config = config;

        // Ensure at least one region exists
        if (regions_.empty()) {
            addRegion();
        }

        // Assign volumes to first region
        obj->regionVolumes[0].push_back(0);

        objects_.push_back(obj);
        meshProcessors_.push_back(std::move(processor));

        std::cout << "BuildPlate: Added model '" << model.path
                  << "' (object " << objects_.size() - 1 << ")" << std::endl;

        // Invalidate arrangement
        invalidateStep(BuildStep::Arrangement);

        return obj;
    } catch (const std::exception& e) {
        std::cerr << "BuildPlate: Error adding model: " << e.what() << std::endl;
        return nullptr;
    }
}

size_t BuildPlate::addModels(const std::vector<InternalModel>& models) {
    size_t count = 0;
    for (const auto& m : models) {
        if (addModel(m)) ++count;
    }
    return count;
}

void BuildPlate::clearObjects() {
    for (auto* obj : objects_) {
        obj->invalidateAllSteps();
        delete obj;
    }
    objects_.clear();
    meshProcessors_.clear();
    clearRegions();
}

void BuildPlate::deleteObject(size_t idx) {
    if (idx >= objects_.size()) return;
    objects_[idx]->invalidateAllSteps();
    delete objects_[idx];
    objects_.erase(objects_.begin() + static_cast<ptrdiff_t>(idx));

    if (idx < meshProcessors_.size()) {
        meshProcessors_.erase(meshProcessors_.begin() +
                              static_cast<ptrdiff_t>(idx));
    }
}

PrintObject* BuildPlate::getObject(size_t idx) {
    return (idx < objects_.size()) ? objects_[idx] : nullptr;
}

const PrintObject* BuildPlate::getObject(size_t idx) const {
    return (idx < objects_.size()) ? objects_[idx] : nullptr;
}

// ==============================================================================
// BuildPlate: Region Management
// ==============================================================================

PrintRegion* BuildPlate::addRegion() {
    auto* region = new PrintRegion(this);
    region->config = config;
    regions_.push_back(region);
    return region;
}

PrintRegion* BuildPlate::getRegion(size_t idx) {
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

const PrintRegion* BuildPlate::getRegion(size_t idx) const {
    return (idx < regions_.size()) ? regions_[idx] : nullptr;
}

void BuildPlate::clearRegions() {
    for (auto* r : regions_) delete r;
    regions_.clear();
}

// ==============================================================================
// BuildPlate: Processing Pipeline
// (Ported from Legacy Print::process)
// ==============================================================================

void BuildPlate::process() {
    reportProgress("Starting build plate processing...", 0);

    // Step 1: Slice all objects
    reportProgress("Slicing objects...", 10);
    for (auto* obj : objects_) {
        obj->slice();
    }

    // Step 2: Add anchor layers
    reportProgress("Adding anchor layers...", 30);
    for (auto* obj : objects_) {
        obj->addAnchorLayers();
    }

    // Step 3: Detect surface types
    reportProgress("Detecting surface types...", 50);
    for (auto* obj : objects_) {
        obj->detectSurfaceTypes();
    }

    // Step 4: Prepare infill
    reportProgress("Preparing infill...", 70);
    for (auto* obj : objects_) {
        obj->prepareInfill();
    }

    // Step 5: Generate support material
    reportProgress("Generating support material...", 85);
    for (auto* obj : objects_) {
        obj->generateSupportMaterial();
    }

    reportProgress("Build plate processing complete", 100);
}

// ==============================================================================
// BuildPlate: Validation
// ==============================================================================

void BuildPlate::validate() const {
    if (objects_.empty()) {
        throw std::runtime_error("BuildPlate: No objects to process");
    }

    if (config.layer_thickness <= 0.0) {
        throw std::runtime_error("BuildPlate: Invalid layer thickness");
    }

    if (config.hatch_spacing <= 0.0) {
        throw std::runtime_error("BuildPlate: Invalid hatch spacing");
    }
}

// ==============================================================================
// BuildPlate: Export to Marc::Layer
// ==============================================================================

std::vector<Marc::Layer> BuildPlate::exportLayers() const {
    // Collect all layers from all objects, sorted by printZ
    struct LayerEntry {
        double printZ;
        double thickness;
        const BuildLayer* layer;
    };

    std::vector<LayerEntry> allEntries;
    for (const auto* obj : objects_) {
        for (size_t i = 0; i < obj->layerCount(); ++i) {
            const auto* layer = obj->getLayer(i);
            if (layer) {
                allEntries.push_back({layer->printZ(), layer->height(), layer});
            }
        }
    }

    // Sort by Z-height
    std::sort(allEntries.begin(), allEntries.end(),
        [](const LayerEntry& a, const LayerEntry& b) {
            return a.printZ < b.printZ;
        });

    // Convert to Marc::Layer format
    std::vector<Marc::Layer> result;
    result.reserve(allEntries.size());

    for (size_t i = 0; i < allEntries.size(); ++i) {
        const auto& entry = allEntries[i];
        Marc::Layer marcLayer(
            static_cast<uint32_t>(i),
            static_cast<float>(entry.printZ),
            static_cast<float>(entry.thickness));

        const auto* buildLayer = entry.layer;

        // Convert BuildLayerRegion data to Marc::Polyline
        for (size_t ri = 0; ri < buildLayer->regionCount(); ++ri) {
            const auto* region = buildLayer->getRegion(ri);
            if (!region) continue;

            // Convert slice contours to Marc polylines
            for (const auto& surf : region->slices) {
                if (!surf.isValid()) continue;

                Marc::Polyline polyline;
                polyline.tag.layerNumber = static_cast<uint32_t>(i);

                // Assign type based on surface classification
                switch (surf.type) {
                    case SurfaceType::Top:
                        polyline.tag.type = Marc::GeometryType::Perimeter;
                        polyline.tag.buildStyle = Marc::BuildStyleID::CoreContour_UpSkin;
                        break;
                    case SurfaceType::Bottom:
                        polyline.tag.type = Marc::GeometryType::Perimeter;
                        polyline.tag.buildStyle = Marc::BuildStyleID::CoreContourOverhang_DownSkin;
                        break;
                    case SurfaceType::Support:
                        polyline.tag.type = Marc::GeometryType::SupportStructure;
                        polyline.tag.buildStyle = Marc::BuildStyleID::SupportContour;
                        break;
                    default:
                        polyline.tag.type = Marc::GeometryType::Perimeter;
                        polyline.tag.buildStyle = Marc::BuildStyleID::CoreContour_Volume;
                        break;
                }

                polyline.points.reserve(surf.contour.size() + 1);
                for (const auto& pt : surf.contour) {
                    float x = static_cast<float>(
                        static_cast<double>(pt.x) * Geometry::MESH_SCALING_FACTOR);
                    float y = static_cast<float>(
                        static_cast<double>(pt.y) * Geometry::MESH_SCALING_FACTOR);
                    polyline.points.emplace_back(x, y);
                }
                // Close the contour
                if (!polyline.points.empty()) {
                    polyline.points.push_back(polyline.points.front());
                }

                marcLayer.polylines.push_back(std::move(polyline));

                // Convert holes
                for (const auto& hole : surf.holes) {
                    Marc::Polyline holeLine;
                    holeLine.tag = polyline.tag;

                    holeLine.points.reserve(hole.size() + 1);
                    for (const auto& pt : hole) {
                        float x = static_cast<float>(
                            static_cast<double>(pt.x) * Geometry::MESH_SCALING_FACTOR);
                        float y = static_cast<float>(
                            static_cast<double>(pt.y) * Geometry::MESH_SCALING_FACTOR);
                        holeLine.points.emplace_back(x, y);
                    }
                    if (!holeLine.points.empty()) {
                        holeLine.points.push_back(holeLine.points.front());
                    }

                    marcLayer.polylines.push_back(std::move(holeLine));
                }
            }

            // Convert support surfaces
            for (const auto& surf : region->supportSurfaces) {
                if (!surf.isValid()) continue;

                Marc::Polyline supportLine;
                supportLine.tag.layerNumber = static_cast<uint32_t>(i);
                supportLine.tag.type = Marc::GeometryType::SupportStructure;
                supportLine.tag.buildStyle = Marc::BuildStyleID::SupportContour;

                supportLine.points.reserve(surf.contour.size() + 1);
                for (const auto& pt : surf.contour) {
                    float x = static_cast<float>(
                        static_cast<double>(pt.x) * Geometry::MESH_SCALING_FACTOR);
                    float y = static_cast<float>(
                        static_cast<double>(pt.y) * Geometry::MESH_SCALING_FACTOR);
                    supportLine.points.emplace_back(x, y);
                }
                if (!supportLine.points.empty()) {
                    supportLine.points.push_back(supportLine.points.front());
                }

                marcLayer.polylines.push_back(std::move(supportLine));
            }
        }

        result.push_back(std::move(marcLayer));
    }

    return result;
}

// ==============================================================================
// BuildPlate: Bounding Box
// ==============================================================================

Geometry::BBox3f BuildPlate::boundingBox() const {
    Geometry::BBox3f bb;
    for (const auto* obj : objects_) {
        const auto& objBB = obj->boundingBox();
        Geometry::Vertex3f minV(
            objBB.min.x + static_cast<float>(obj->placement.x),
            objBB.min.y + static_cast<float>(obj->placement.y),
            objBB.min.z + static_cast<float>(obj->placement.z));
        Geometry::Vertex3f maxV(
            objBB.max.x + static_cast<float>(obj->placement.x),
            objBB.max.y + static_cast<float>(obj->placement.y),
            objBB.max.z + static_cast<float>(obj->placement.z));
        bb.merge(minV);
        bb.merge(maxV);
    }
    return bb;
}

// ==============================================================================
// BuildPlate: State
// ==============================================================================

bool BuildPlate::invalidateStep(BuildStep step) {
    return state.invalidate(step);
}

bool BuildPlate::invalidateAllSteps() {
    bool invalidated = state.invalidateAll();
    for (auto* obj : objects_) {
        invalidated |= obj->invalidateAllSteps();
    }
    return invalidated;
}

// ==============================================================================
// BuildPlate: Progress
// ==============================================================================

void BuildPlate::reportProgress(const char* msg, int pct) {
    if (progressCb_) progressCb_(msg, pct);
    std::cout << "  [" << pct << "%] " << msg << std::endl;
}

} // namespace MarcSLM
