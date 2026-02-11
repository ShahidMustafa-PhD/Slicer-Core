#pragma once
#include <vector>
#include <memory>
#include "../libslic3r/Print.hpp"
#include "../libslic3r/Layer.hpp"
#include "../libslic3r/SurfaceCollection.hpp"
#include "../libslic3r/ExtrusionEntityCollection.hpp"
#include "../libslic3r/Polyline.hpp"
#include "../libslic3r/Polygon.hpp"
#include "../libslic3r/ExPolygon.hpp"
#include "../libslic3r/SlmHatch.hpp"

namespace Marc {

class scanSegments {
public:
    explicit scanSegments(Slic3r::PrintObject* printObject, const SlmConfig& config);
    ~scanSegments() = default;

    void segment_all();

private:
    Slic3r::PrintObject* m_object;
    SlmConfig m_cfg;

    void segment_layer(const Slic3r::Layer* layer);
    void segment_layer_regions(const Slic3r::LayerRegionPtrs& regions);

    void segment_hatches_region(Slic3r::LayerRegion* region);
    void segment_perimeters_region(Slic3r::LayerRegion* region);
    void segment_fills_region(Slic3r::LayerRegion* region);
    void segment_support_surfaces_region(Slic3r::LayerRegion* region);

    void segment_support_layer(const Slic3r::SupportLayer* layer);

    static Slic3r::Polylines polygon_to_polylines(const Slic3r::Polygon& poly);
    static Slic3r::Polylines expolygon_to_polylines(const Slic3r::ExPolygon& exp);

    static Marc::ThermalSegmentType role_to_segment(Slic3r::ExtrusionRole role);
};

} // namespace Marc
