// ==============================================================================
// MarcSLM - Scan Segment Classifier Implementation
// ==============================================================================
// Orchestrates the PySLM-style thermal classification pipeline:
//
//   Stage 1: LayerPolygonExtractor  -> Marc::Layer -> Clipper2 Paths64
//   Stage 2: ThermalMaskGenerator   -> Volume / Overhang masks
//   Stage 3: ShellDecomposer        -> Shell1 / Shell2 / Core
//   Stage 4: ContourHatchSplitter   -> Contour / Hatch per region
//   Stage 5: RegionClassifier       -> Intersect -> 22 ThermalSegmentType
//   Stage 6: ThermalRegionHatcher   -> Parallel scan vectors per region
//
// TBB is used to parallelise Stage 1 (polygon extraction) and the per-layer
// classification + hatching loop (Stages 2-6).
// ==============================================================================

#include "MarcSLM/Thermal/ScanSegmentClassifier.hpp"

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#include <algorithm>
#include <cmath>

namespace MarcSLM {

// ==============================================================================
// Internal helpers (anonymous namespace, defined before first use)
// ==============================================================================

namespace {

/// @brief Returns true if the ThermalSegmentType represents a contour
///        (output as polylines) rather than a hatch (output as line segments).
bool isContourType(ThermalSegmentType t) noexcept {
    switch (t) {
        case ThermalSegmentType::CoreContour_Volume:
        case ThermalSegmentType::CoreContour_Overhang:
        case ThermalSegmentType::HollowShell1Contour_Volume:
        case ThermalSegmentType::HollowShell1Contour_Overhang:
        case ThermalSegmentType::HollowShell2Contour_Volume:
        case ThermalSegmentType::HollowShell2Contour_Overhang:
        case ThermalSegmentType::SupportContourVolume:
            return true;
        default:
            return false;
    }
}

/// @brief Returns true if the type represents a hatch region that should
///        receive parallel scan vectors (not a contour boundary).
bool isHatchType(ThermalSegmentType t) noexcept {
    switch (t) {
        case ThermalSegmentType::CoreNormalHatch:
        case ThermalSegmentType::CoreOverhangHatch:
        case ThermalSegmentType::CoreContourHatch:
        case ThermalSegmentType::CoreContourHatchOverhang:
        case ThermalSegmentType::HollowShell1NormalHatch:
        case ThermalSegmentType::HollowShell1OverhangHatch:
        case ThermalSegmentType::HollowShell1ContourHatch:
        case ThermalSegmentType::HollowShell1ContourHatchOverhang:
        case ThermalSegmentType::HollowShell2NormalHatch:
        case ThermalSegmentType::HollowShell2OverhangHatch:
        case ThermalSegmentType::HollowShell2ContourHatch:
        case ThermalSegmentType::HollowShell2ContourHatchOverhang:
        case ThermalSegmentType::SupportHatch:
        case ThermalSegmentType::ExternalSupports:
            return true;
        default:
            return false;
    }
}

} // anonymous namespace

// ==============================================================================
// Construction
// ==============================================================================

ScanSegmentClassifier::ScanSegmentClassifier(const SlmConfig& config)
    : config_(config)
    , params_()
{
    params_.shell1Thickness     = config.perimeter_hatch_spacing;
    params_.shell2Thickness     = config.perimeter_hatch_spacing;
    params_.contourWidth        = config.beam_diameter;
    params_.enableParallel      = (config.threads > 1);
    params_.generateHatchVectors = true;
}

ScanSegmentClassifier::ScanSegmentClassifier(const SlmConfig& config,
                                             const SegmentationParams& params)
    : config_(config)
    , params_(params)
{
}

// ==============================================================================
// Public API: classifyAll (TBB-parallelised)
// ==============================================================================

std::vector<ClassifiedLayer>
ScanSegmentClassifier::classifyAll(const std::vector<Marc::Layer>& layers) const
{
    const size_t N = layers.size();
    if (N == 0) return {};

    // ---- Stage 1: Extract Clipper2 Paths64 for every layer ----------------
    // This is embarrassingly parallel: each layer is independent.
    Thermal::LayerPolygonExtractor extractor(params_.clipperScale);
    std::vector<Clipper2Lib::Paths64> layerPaths(N);

    auto extractBody = [&](const tbb::blocked_range<size_t>& range) {
        for (size_t i = range.begin(); i < range.end(); ++i) {
            layerPaths[i] = extractor.extract(layers[i]);
        }
    };

    if (params_.enableParallel && N > 1) {
        tbb::parallel_for(tbb::blocked_range<size_t>(0, N), extractBody);
    } else {
        extractBody(tbb::blocked_range<size_t>(0, N));
    }

    // ---- Stages 2-6: Per-layer classification + hatching ------------------
    // Each layer depends on its own Paths64 + the previous layer's Paths64.
    // This is still parallel because the dependency is read-only.
    std::vector<ClassifiedLayer> result(N);

    auto classifyBody = [&](const tbb::blocked_range<size_t>& range) {
        for (size_t i = range.begin(); i < range.end(); ++i) {
            const Clipper2Lib::Paths64* prev =
                (i > 0) ? &layerPaths[i - 1] : nullptr;
            result[i] = classifyLayerImpl(layers[i], layerPaths[i], prev);
        }
    };

    if (params_.enableParallel && N > 1) {
        tbb::parallel_for(tbb::blocked_range<size_t>(0, N), classifyBody);
    } else {
        classifyBody(tbb::blocked_range<size_t>(0, N));
    }

    return result;
}

// ==============================================================================
// Public API: classifyLayer (single layer, no overhang)
// ==============================================================================

ClassifiedLayer
ScanSegmentClassifier::classifyLayer(const Marc::Layer& layer) const
{
    Thermal::LayerPolygonExtractor extractor(params_.clipperScale);
    auto paths = extractor.extract(layer);
    return classifyLayerImpl(layer, paths, nullptr);
}

// ==============================================================================
// Core Pipeline: classifyLayerImpl (Stages 2-6)
// ==============================================================================

ClassifiedLayer
ScanSegmentClassifier::classifyLayerImpl(
    const Marc::Layer& layer,
    const Clipper2Lib::Paths64& currentPaths,
    const Clipper2Lib::Paths64* prevPaths) const
{
    ClassifiedLayer classified;
    classified.layerNumber    = layer.layerNumber;
    classified.layerHeight    = layer.layerHeight;
    classified.layerThickness = layer.layerThickness;
    classified.geometry       = layer;

    // ---- Stage 0: Passthrough pre-tagged geometry -------------------------
    passExistingHatches(layer, classified);
    passExistingPolylines(layer, classified);
    passExistingPolygons(layer, classified);

    if (currentPaths.empty()) return classified;

    // ---- Stage 2: Thermal masks (Volume / Overhang) -----------------------
    auto masks = Thermal::ThermalMaskGenerator::compute(currentPaths, prevPaths);

    // ---- Stage 3: Shell decomposition -------------------------------------
    const double scale = params_.clipperScale;
    Thermal::ShellDecomposer shellDecomp(
        params_.shell1Thickness * scale,
        params_.shell2Thickness * scale,
        params_.miterLimit);

    auto shells = shellDecomp.decompose(currentPaths);

    // ---- Stage 4: Contour / Hatch split per region ------------------------
    Thermal::ContourHatchSplitter splitter(
        params_.contourWidth * scale,
        params_.contourHatchFraction,
        params_.miterLimit);

    auto coreSplit   = splitter.split(shells.core);
    auto shell1Split = splitter.split(shells.shell1);
    auto shell2Split = splitter.split(shells.shell2);

    // ---- Stage 5: Region classification (22-type matrix) ------------------
    Thermal::RegionClassifier::PhysicalRegions phys;
    phys.coreContour       = &coreSplit.contour;
    phys.coreHatch         = &coreSplit.hatch;
    phys.coreContourHatch  = &coreSplit.contourHatch;
    phys.shell1Contour     = &shell1Split.contour;
    phys.shell1Hatch       = &shell1Split.hatch;
    phys.shell1ContourHatch= &shell1Split.contourHatch;
    phys.shell2Contour     = &shell2Split.contour;
    phys.shell2Hatch       = &shell2Split.hatch;
    phys.shell2ContourHatch= &shell2Split.contourHatch;

    auto taggedRegions = Thermal::RegionClassifier::classifyAll(
        phys, masks.volume, masks.overhang);

    // ---- Stage 6: Generate scan vectors + convert to Marc output ----------
    Thermal::LayerPolygonExtractor converter(params_.clipperScale);

    // Build the hatcher (thread-local, no shared state)
    Thermal::ThermalRegionHatcher hatcher(config_);
    hatcher.setClipperScale(params_.clipperScale);
    hatcher.setEndpointOverlap(params_.endpointOverlap);

    for (auto& tr : taggedRegions) {
        if (isContourType(tr.type)) {
            // Contour types -> polyline output (boundary path, no infill)
            converter.toPolylines(tr.paths, tr.type,
                                  layer.layerNumber,
                                  classified.segmentPolylines);
        } else if (isHatchType(tr.type) && params_.generateHatchVectors) {
            // Hatch types -> generate actual parallel scan vectors
            auto hatchLines = hatcher.hatchRegion(
                tr.paths, tr.type, layer.layerNumber);

            if (!hatchLines.empty()) {
                ScanSegmentHatch seg;
                seg.type    = tr.type;
                seg.hatches = std::move(hatchLines);
                classified.segmentHatches.push_back(std::move(seg));
            }
        } else {
            // Fallback: convert boundary edges as hatch lines
            converter.toHatches(tr.paths, tr.type,
                                layer.layerNumber,
                                classified.segmentHatches);
        }

        // Store the Clipper2 region for downstream use
        classified.regions.push_back({tr.type, std::move(tr.paths)});
    }

    return classified;
}

// ==============================================================================
// Passthrough Classification (preserves existing tags)
// ==============================================================================

void ScanSegmentClassifier::passExistingHatches(
    const Marc::Layer& layer,
    ClassifiedLayer& out)
{
    for (const auto& hatch : layer.hatches) {
        ScanSegmentHatch seg;
        seg.type    = mapToThermalSegment(hatch.tag.type, hatch.tag.buildStyle);
        seg.hatches = hatch.lines;
        out.segmentHatches.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::passExistingPolylines(
    const Marc::Layer& layer,
    ClassifiedLayer& out)
{
    for (const auto& polyline : layer.polylines) {
        ScanSegmentPolyline seg;
        seg.type = mapToThermalSegment(polyline.tag.type, polyline.tag.buildStyle);

        Marc::Polyline pl;
        pl.points = polyline.points;
        pl.tag    = polyline.tag;
        seg.polylines.push_back(std::move(pl));

        out.segmentPolylines.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::passExistingPolygons(
    const Marc::Layer& layer,
    ClassifiedLayer& out)
{
    for (const auto& polygon : layer.polygons) {
        ScanSegmentPolyline seg;
        seg.type = mapToThermalSegment(polygon.tag.type, polygon.tag.buildStyle);

        Marc::Polyline pl;
        pl.points = polygon.points;
        if (!pl.points.empty()) {
            pl.points.push_back(pl.points.front());
        }
        pl.tag = polygon.tag;
        seg.polylines.push_back(std::move(pl));

        out.segmentPolylines.push_back(std::move(seg));
    }
}

// ==============================================================================
// BuildStyleID -> ThermalSegmentType Mapping
// ==============================================================================

ThermalSegmentType
ScanSegmentClassifier::mapToThermalSegment(Marc::GeometryType gtype,
                                           Marc::BuildStyleID bstyle) noexcept
{
    switch (bstyle) {
        case Marc::BuildStyleID::CoreContour_Volume:
            return ThermalSegmentType::CoreContour_Volume;
        case Marc::BuildStyleID::CoreHatch_Volume:
            return ThermalSegmentType::CoreNormalHatch;
        case Marc::BuildStyleID::Shell1Contour_Volume:
            return ThermalSegmentType::HollowShell1Contour_Volume;
        case Marc::BuildStyleID::Shell1Hatch_Volume:
            return ThermalSegmentType::HollowShell1NormalHatch;
        case Marc::BuildStyleID::Shell2Contour_Volume:
            return ThermalSegmentType::HollowShell2Contour_Volume;
        case Marc::BuildStyleID::Shell2Hatch_Volume:
            return ThermalSegmentType::HollowShell2NormalHatch;
        case Marc::BuildStyleID::CoreContour_UpSkin:
            return ThermalSegmentType::CoreContour_Volume;
        case Marc::BuildStyleID::CoreHatch_UpSkin:
            return ThermalSegmentType::CoreNormalHatch;
        case Marc::BuildStyleID::Shell1Contour_UpSkin:
            return ThermalSegmentType::HollowShell1Contour_Volume;
        case Marc::BuildStyleID::Shell1Hatch_UpSkin:
            return ThermalSegmentType::HollowShell1NormalHatch;
        case Marc::BuildStyleID::CoreContourOverhang_DownSkin:
            return ThermalSegmentType::CoreContour_Overhang;
        case Marc::BuildStyleID::CoreHatchOverhang_DownSkin:
            return ThermalSegmentType::CoreOverhangHatch;
        case Marc::BuildStyleID::Shell1ContourOverhang_DownSkin:
            return ThermalSegmentType::HollowShell1Contour_Overhang;
        case Marc::BuildStyleID::Shell1HatchOverhang_DownSkin:
            return ThermalSegmentType::HollowShell1OverhangHatch;
        case Marc::BuildStyleID::HollowShell1Contour:
            return ThermalSegmentType::HollowShell1Contour_Volume;
        case Marc::BuildStyleID::HollowShell1ContourHatch:
            return ThermalSegmentType::HollowShell1ContourHatch;
        case Marc::BuildStyleID::HollowShell1ContourHatchOverhang:
            return ThermalSegmentType::HollowShell1ContourHatchOverhang;
        case Marc::BuildStyleID::HollowShell2Contour:
            return ThermalSegmentType::HollowShell2Contour_Volume;
        case Marc::BuildStyleID::HollowShell2ContourHatch:
            return ThermalSegmentType::HollowShell2ContourHatch;
        case Marc::BuildStyleID::HollowShell2ContourHatchOverhang:
            return ThermalSegmentType::HollowShell2ContourHatchOverhang;
        case Marc::BuildStyleID::SupportStructure:
            return ThermalSegmentType::SupportHatch;
        case Marc::BuildStyleID::SupportContour:
            return ThermalSegmentType::SupportContourVolume;
        default:
            break;
    }

    switch (gtype) {
        case Marc::GeometryType::CoreHatch:
            return ThermalSegmentType::CoreNormalHatch;
        case Marc::GeometryType::OverhangHatch:
            return ThermalSegmentType::CoreOverhangHatch;
        case Marc::GeometryType::Perimeter:
            return ThermalSegmentType::CoreContour_Volume;
        case Marc::GeometryType::SupportStructure:
            return ThermalSegmentType::ExternalSupports;
        case Marc::GeometryType::InfillPattern:
            return ThermalSegmentType::CoreContourHatch;
        default:
            return ThermalSegmentType::CoreContour_Volume;
    }
}

} // namespace MarcSLM
