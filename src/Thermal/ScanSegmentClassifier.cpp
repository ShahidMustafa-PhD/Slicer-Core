// ==============================================================================
// MarcSLM - Scan Segment Classifier Implementation
// ==============================================================================
// Ported from Legacy scanSegments.cpp
// ==============================================================================

#include "MarcSLM/Thermal/ScanSegmentClassifier.hpp"

#include <iostream>

namespace MarcSLM {

ScanSegmentClassifier::ScanSegmentClassifier(const SlmConfig& config)
    : config_(config) {
}

std::vector<ClassifiedLayer>
ScanSegmentClassifier::classifyAll(const std::vector<Marc::Layer>& layers) const {
    std::vector<ClassifiedLayer> result;
    result.reserve(layers.size());

    for (const auto& layer : layers) {
        result.push_back(classifyLayer(layer));
    }

    return result;
}

ClassifiedLayer
ScanSegmentClassifier::classifyLayer(const Marc::Layer& layer) const {
    ClassifiedLayer classified;
    classified.layerNumber = layer.layerNumber;
    classified.layerHeight = layer.layerHeight;
    classified.layerThickness = layer.layerThickness;
    classified.geometry = layer;  // copy the raw geometry

    classifyHatches(layer, classified);
    classifyPolylines(layer, classified);
    classifyPolygons(layer, classified);

    return classified;
}

void ScanSegmentClassifier::classifyHatches(const Marc::Layer& layer,
                                             ClassifiedLayer& classified) const {
    for (const auto& hatch : layer.hatches) {
        ScanSegmentHatch seg;
        seg.type = mapToThermalSegment(hatch.tag.type, hatch.tag.buildStyle);

        // Convert Marc::Line to ScanSegmentHatch::hatches
        seg.hatches = hatch.lines;

        classified.segmentHatches.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::classifyPolylines(const Marc::Layer& layer,
                                               ClassifiedLayer& classified) const {
    for (const auto& polyline : layer.polylines) {
        ScanSegmentPolyline seg;
        seg.type = mapToThermalSegment(polyline.tag.type, polyline.tag.buildStyle);

        Marc::Polyline pl;
        pl.points = polyline.points;
        pl.tag = polyline.tag;
        seg.polylines.push_back(std::move(pl));

        classified.segmentPolylines.push_back(std::move(seg));
    }
}

void ScanSegmentClassifier::classifyPolygons(const Marc::Layer& layer,
                                              ClassifiedLayer& classified) const {
    for (const auto& polygon : layer.polygons) {
        // Convert polygon to polyline segment for classification
        ScanSegmentPolyline seg;
        seg.type = mapToThermalSegment(polygon.tag.type, polygon.tag.buildStyle);

        // Create a polyline from the polygon points (close it)
        Marc::Polyline pl;
        pl.points = polygon.points;
        if (!pl.points.empty()) {
            pl.points.push_back(pl.points.front()); // close the polygon
        }
        pl.tag = polygon.tag;
        seg.polylines.push_back(std::move(pl));

        classified.segmentPolylines.push_back(std::move(seg));
    }
}

ThermalSegmentType
ScanSegmentClassifier::mapToThermalSegment(Marc::GeometryType gtype,
                                            Marc::BuildStyleID bstyle) const {
    // Map BuildStyleID to ThermalSegmentType
    // This mapping follows the legacy scanSegments::role_to_segment logic
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
            // Fallback: classify based on GeometryType
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
}

} // namespace MarcSLM
