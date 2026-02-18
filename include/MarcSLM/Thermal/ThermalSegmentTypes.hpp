// ==============================================================================
// MarcSLM - Thermal Segment Type Classification
// ==============================================================================
// Ported from Legacy Layer.hpp Marc::ThermalSegmentType
// 22-type industrial thermal classification for SLM scan vectors
// ==============================================================================

#pragma once

#include "MarcSLM/Core/MarcFormat.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace MarcSLM {

/// @brief Complete thermal-segment taxonomy for DMLS/SLM machines.
/// @details Each value maps 1:1 to a laser parameter set (BuildStyle).
///          The segmenter classifies every scan vector (hatch line, perimeter
///          polyline, or polygon contour) into one of these 22 categories based
///          on geometry region (core / hollow-shell-1 / hollow-shell-2),
///          surface orientation (volume / overhang), and vector role
///          (contour / hatch).
///
/// Ported from Legacy: Marc::ThermalSegmentType (Layer.hpp)
enum class ThermalSegmentType : uint32_t {
    CoreContour_Volume             =  1,
    CoreContour_Overhang           =  2,
    HollowShell1Contour_Volume     =  3,
    HollowShell1Contour_Overhang   =  4,
    HollowShell2Contour_Volume     =  5,
    HollowShell2Contour_Overhang   =  6,
    CoreOverhangHatch              =  7,
    CoreNormalHatch                =  8,
    CoreContourHatch               =  9,
    HollowShell1OverhangHatch      = 10,
    HollowShell1NormalHatch        = 11,
    HollowShell1ContourHatch       = 12,
    HollowShell2OverhangHatch      = 13,
    HollowShell2NormalHatch        = 14,
    HollowShell2ContourHatch       = 15,
    SupportContourVolume           = 16,
    SupportHatch                   = 17,
    PointSequence                  = 18,
    ExternalSupports               = 19,
    CoreContourHatchOverhang       = 20,
    HollowShell1ContourHatchOverhang = 21,
    HollowShell2ContourHatchOverhang = 22
};

/// @brief Convert ThermalSegmentType to a human-readable string.
[[nodiscard]] inline const char* thermalSegmentToString(ThermalSegmentType t) noexcept {
    switch (t) {
        case ThermalSegmentType::CoreContour_Volume:             return "CoreContour_Volume";
        case ThermalSegmentType::CoreContour_Overhang:           return "CoreContour_Overhang";
        case ThermalSegmentType::HollowShell1Contour_Volume:     return "HollowShell1Contour_Volume";
        case ThermalSegmentType::HollowShell1Contour_Overhang:   return "HollowShell1Contour_Overhang";
        case ThermalSegmentType::HollowShell2Contour_Volume:     return "HollowShell2Contour_Volume";
        case ThermalSegmentType::HollowShell2Contour_Overhang:   return "HollowShell2Contour_Overhang";
        case ThermalSegmentType::CoreOverhangHatch:              return "CoreOverhangHatch";
        case ThermalSegmentType::CoreNormalHatch:                return "CoreNormalHatch";
        case ThermalSegmentType::CoreContourHatch:               return "CoreContourHatch";
        case ThermalSegmentType::HollowShell1OverhangHatch:      return "HollowShell1OverhangHatch";
        case ThermalSegmentType::HollowShell1NormalHatch:        return "HollowShell1NormalHatch";
        case ThermalSegmentType::HollowShell1ContourHatch:       return "HollowShell1ContourHatch";
        case ThermalSegmentType::HollowShell2OverhangHatch:      return "HollowShell2OverhangHatch";
        case ThermalSegmentType::HollowShell2NormalHatch:        return "HollowShell2NormalHatch";
        case ThermalSegmentType::HollowShell2ContourHatch:       return "HollowShell2ContourHatch";
        case ThermalSegmentType::SupportContourVolume:           return "SupportContourVolume";
        case ThermalSegmentType::SupportHatch:                   return "SupportHatch";
        case ThermalSegmentType::PointSequence:                  return "PointSequence";
        case ThermalSegmentType::ExternalSupports:               return "ExternalSupports";
        case ThermalSegmentType::CoreContourHatchOverhang:       return "CoreContourHatchOverhang";
        case ThermalSegmentType::HollowShell1ContourHatchOverhang: return "HollowShell1ContourHatchOverhang";
        case ThermalSegmentType::HollowShell2ContourHatchOverhang: return "HollowShell2ContourHatchOverhang";
        default: return "Unknown";
    }
}

/// @brief Get a display color for a ThermalSegmentType (for SVG visualization).
/// @details Matches the color scheme from the legacy SlmPrint::get_color_for_type.
[[nodiscard]] inline const char* thermalSegmentColor(ThermalSegmentType t) noexcept {
    switch (t) {
        case ThermalSegmentType::CoreContour_Volume:             return "black";
        case ThermalSegmentType::CoreContour_Overhang:           return "maroon";
        case ThermalSegmentType::HollowShell1Contour_Volume:     return "blue";
        case ThermalSegmentType::HollowShell1Contour_Overhang:   return "darkblue";
        case ThermalSegmentType::HollowShell2Contour_Volume:     return "yellow";
        case ThermalSegmentType::HollowShell2Contour_Overhang:   return "darkgreen";
        case ThermalSegmentType::CoreOverhangHatch:              return "orange";
        case ThermalSegmentType::CoreNormalHatch:                return "green";
        case ThermalSegmentType::CoreContourHatch:               return "purple";
        case ThermalSegmentType::HollowShell1OverhangHatch:      return "pink";
        case ThermalSegmentType::HollowShell1NormalHatch:        return "cyan";
        case ThermalSegmentType::HollowShell1ContourHatch:       return "magenta";
        case ThermalSegmentType::HollowShell2OverhangHatch:      return "brown";
        case ThermalSegmentType::HollowShell2NormalHatch:        return "gray";
        case ThermalSegmentType::HollowShell2ContourHatch:       return "lime";
        case ThermalSegmentType::SupportContourVolume:           return "navy";
        case ThermalSegmentType::SupportHatch:                   return "olive";
        case ThermalSegmentType::PointSequence:                  return "teal";
        case ThermalSegmentType::ExternalSupports:               return "red";
        case ThermalSegmentType::CoreContourHatchOverhang:       return "coral";
        case ThermalSegmentType::HollowShell1ContourHatchOverhang: return "indigo";
        case ThermalSegmentType::HollowShell2ContourHatchOverhang: return "violet";
        default: return "blue";
    }
}

// ==============================================================================
// Scan Segment Data Structures
// ==============================================================================

/// @brief Classified group of hatch lines sharing a thermal segment type.
/// @details Replaces Legacy Marc::ScanSegmentHatch.
///          Uses Marc::Line from MarcFormat.hpp for line storage.
struct ScanSegmentHatch {
    std::vector<Marc::Line> hatches; ///< Hatch line segments
    ThermalSegmentType type;         ///< Thermal classification
    
    ScanSegmentHatch() noexcept
        : type(ThermalSegmentType::CoreNormalHatch) {}
};

/// @brief Classified group of polylines sharing a thermal segment type.
/// @details Replaces Legacy Marc::ScanSegmentPolyline.
///          Uses Marc::Point from MarcFormat.hpp for point storage.
struct ScanSegmentPolyline {
    std::vector<Marc::Polyline> polylines; ///< Open-path polylines
    ThermalSegmentType type;               ///< Thermal classification
    
    ScanSegmentPolyline() noexcept
        : type(ThermalSegmentType::CoreContour_Volume) {}
};

} // namespace MarcSLM
