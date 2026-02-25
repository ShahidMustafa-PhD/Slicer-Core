// ==============================================================================
// MarcSLM - Region Classifier
// ==============================================================================
// Stage 5 of PySLM pipeline: Intersect physical regions with thermal masks
// to produce the final 22 ThermalSegmentType zones.
//
// Matrix (9 physical regions x 2 thermal masks = 18 core types + 4 extras):
//
// | Physical Region      | x Volume         | x Overhang            |
// |----------------------|------------------|-----------------------|
// | Core Contour         | 1  CoreContV     | 2  CoreContOH         |
// | Core Hatch           | 8  CoreNormH     | 7  CoreOHHatch        |
// | Core ContourHatch    | 9  CoreCH        | 20 CoreCHOH           |
// | Shell1 Contour       | 3  S1ContV       | 4  S1ContOH           |
// | Shell1 Hatch         | 11 S1NormH       | 10 S1OHHatch          |
// | Shell1 ContourHatch  | 12 S1CH          | 21 S1CHOH             |
// | Shell2 Contour       | 5  S2ContV       | 6  S2ContOH           |
// | Shell2 Hatch         | 14 S2NormH       | 13 S2OHHatch          |
// | Shell2 ContourHatch  | 15 S2CH          | 22 S2CHOH             |
//
// Support types (16, 17, 18, 19) are handled by tag passthrough.
//
// Thread-safe: all methods are static with no shared mutable state.
// ==============================================================================

#pragma once

#include "MarcSLM/Thermal/ClipperBoolean.hpp"
#include "MarcSLM/Thermal/ThermalSegmentTypes.hpp"

#include <clipper2/clipper.h>

#include <vector>

namespace MarcSLM {
namespace Thermal {

/// @brief A classified polygonal region tagged with a ThermalSegmentType.
struct TaggedRegion {
    ThermalSegmentType   type;
    Clipper2Lib::Paths64 paths;
};

/// @brief Intersects physical regions with thermal masks to assign the
///        final 22 ThermalSegmentType values.
class RegionClassifier {
public:
    /// @brief Classify a physical region against volume and overhang masks.
    /// @param region       The physical region polygon set.
    /// @param volumeMask   Volume (inskin) mask.
    /// @param overhangMask Overhang (downskin) mask.
    /// @param volumeType   ThermalSegmentType for the volume intersection.
    /// @param overhangType ThermalSegmentType for the overhang intersection.
    /// @param out          Output vector to append classified regions.
    static void classify(
        const Clipper2Lib::Paths64& region,
        const Clipper2Lib::Paths64& volumeMask,
        const Clipper2Lib::Paths64& overhangMask,
        ThermalSegmentType volumeType,
        ThermalSegmentType overhangType,
        std::vector<TaggedRegion>& out)
    {
        if (region.empty()) return;

        if (!volumeMask.empty()) {
            auto vol = ClipperBoolean::intersect(region, volumeMask);
            if (!vol.empty()) {
                out.push_back({volumeType, std::move(vol)});
            }
        }

        if (!overhangMask.empty()) {
            auto oh = ClipperBoolean::intersect(region, overhangMask);
            if (!oh.empty()) {
                out.push_back({overhangType, std::move(oh)});
            }
        }
    }

    /// @brief Run the full 22-type classification matrix.
    /// @details Applies the PySLM intersection matrix to all 9 physical regions
    ///          (3 shells x {contour, hatch, contourHatch}) against both thermal
    ///          masks, producing up to 18 tagged regions per layer.
    struct PhysicalRegions {
        // Core
        const Clipper2Lib::Paths64* coreContour      = nullptr;
        const Clipper2Lib::Paths64* coreHatch         = nullptr;
        const Clipper2Lib::Paths64* coreContourHatch  = nullptr;
        // Shell 1
        const Clipper2Lib::Paths64* shell1Contour     = nullptr;
        const Clipper2Lib::Paths64* shell1Hatch       = nullptr;
        const Clipper2Lib::Paths64* shell1ContourHatch= nullptr;
        // Shell 2
        const Clipper2Lib::Paths64* shell2Contour     = nullptr;
        const Clipper2Lib::Paths64* shell2Hatch       = nullptr;
        const Clipper2Lib::Paths64* shell2ContourHatch= nullptr;
    };

    [[nodiscard]] static std::vector<TaggedRegion>
    classifyAll(const PhysicalRegions& phys,
                const Clipper2Lib::Paths64& volumeMask,
                const Clipper2Lib::Paths64& overhangMask)
    {
        std::vector<TaggedRegion> out;
        out.reserve(18);  // Up to 9 regions x 2 masks

        // Helper macro to reduce boilerplate
        #define CLASSIFY(ptr, volT, ohT)                           \
            if ((ptr) != nullptr && !(ptr)->empty())               \
                classify(*(ptr), volumeMask, overhangMask, (volT), (ohT), out)

        // Core
        CLASSIFY(phys.coreContour,
                 ThermalSegmentType::CoreContour_Volume,
                 ThermalSegmentType::CoreContour_Overhang);
        CLASSIFY(phys.coreHatch,
                 ThermalSegmentType::CoreNormalHatch,
                 ThermalSegmentType::CoreOverhangHatch);
        CLASSIFY(phys.coreContourHatch,
                 ThermalSegmentType::CoreContourHatch,
                 ThermalSegmentType::CoreContourHatchOverhang);

        // Shell 1
        CLASSIFY(phys.shell1Contour,
                 ThermalSegmentType::HollowShell1Contour_Volume,
                 ThermalSegmentType::HollowShell1Contour_Overhang);
        CLASSIFY(phys.shell1Hatch,
                 ThermalSegmentType::HollowShell1NormalHatch,
                 ThermalSegmentType::HollowShell1OverhangHatch);
        CLASSIFY(phys.shell1ContourHatch,
                 ThermalSegmentType::HollowShell1ContourHatch,
                 ThermalSegmentType::HollowShell1ContourHatchOverhang);

        // Shell 2
        CLASSIFY(phys.shell2Contour,
                 ThermalSegmentType::HollowShell2Contour_Volume,
                 ThermalSegmentType::HollowShell2Contour_Overhang);
        CLASSIFY(phys.shell2Hatch,
                 ThermalSegmentType::HollowShell2NormalHatch,
                 ThermalSegmentType::HollowShell2OverhangHatch);
        CLASSIFY(phys.shell2ContourHatch,
                 ThermalSegmentType::HollowShell2ContourHatch,
                 ThermalSegmentType::HollowShell2ContourHatchOverhang);

        #undef CLASSIFY

        return out;
    }
};

} // namespace Thermal
} // namespace MarcSLM
