#include "scanSegments.hpp"
#include "ExportMarcSlice.hpp"
#include <iostream>

namespace Marc {

scanSegments::scanSegments(Slic3r::PrintObject* printObject, const SlmConfig& config)
    : m_object(printObject), m_cfg(config) {}

void scanSegments::segment_all()
{
    if (!m_object) {
        std::cerr << "scanSegments: null PrintObject" << std::endl;
        return;
    }

    // Process model layers
    for (const Slic3r::Layer* layer : m_object->layers) {
        segment_layer(layer);
    }

    // Process support layers as ExternalSupports
    for (const Slic3r::SupportLayer* s : m_object->support_layers) {
        segment_support_layer(s);
    }
}

void scanSegments::segment_layer(const Slic3r::Layer* layer)
{
    if (!layer) return;
    segment_layer_regions(layer->regions);
}

void scanSegments::segment_layer_regions(const Slic3r::LayerRegionPtrs& regions)
{
    for (Slic3r::LayerRegion* region : regions) {
        if (!region) continue;
        segment_hatches_region(region);
        segment_perimeters_region(region);
        segment_fills_region(region);
        segment_support_surfaces_region(region);
    }
}

void scanSegments::segment_hatches_region(Slic3r::LayerRegion* region)
{
    if (region->hatches.empty()) return;
    // Map SLM hatches into Marc::ScanSegment items attached to the region
    for (const Slic3r::SlmHatch& h : region->hatches) {
        Marc::ScanSegmentHatch seg;
        seg.hatches = h.lines();
        // Map surface type to thermal segment category for hatches
        seg.type = (h.surface_type == Slic3r::stBridge)
                     ? Marc::CoreOverhangHatch
                     : Marc::CoreNormalHatch;
        region->segment_hatches_.push_back(std::move(seg));
    }
}

void scanSegments::segment_perimeters_region(Slic3r::LayerRegion* region)
{
    if (region->perimeters.entities.empty()) return;
    for (const Slic3r::ExtrusionEntity* e : region->perimeters.entities) {
        if (const Slic3r::ExtrusionPath* path = dynamic_cast<const Slic3r::ExtrusionPath*>(e)) {
            Marc::ScanSegmentPolyline seg;
            seg.polys = Slic3r::Polylines{ path->polyline };
            seg.type = role_to_segment(path->get_role());
            region->segment_polylines_.push_back(std::move(seg));
        } else if (const Slic3r::ExtrusionLoop* loop = dynamic_cast<const Slic3r::ExtrusionLoop*>(e)) {
            for (const Slic3r::ExtrusionPath& p : loop->paths) {
                Marc::ScanSegmentPolyline seg;
                seg.polys = Slic3r::Polylines{ p.polyline };
                seg.type = role_to_segment(p.get_role());
                region->segment_polylines_.push_back(std::move(seg));
            }
        } else if (const Slic3r::ExtrusionEntityCollection* coll = dynamic_cast<const Slic3r::ExtrusionEntityCollection*>(e)) {
            for (const Slic3r::ExtrusionEntity* sub : coll->entities) {
                if (const Slic3r::ExtrusionPath* path = dynamic_cast<const Slic3r::ExtrusionPath*>(sub)) {
                    Marc::ScanSegmentPolyline seg;
                    seg.polys = Slic3r::Polylines{ path->polyline };
                    seg.type = role_to_segment(path->get_role());
                    region->segment_polylines_.push_back(std::move(seg));
                }
            }
        }
    }
}

void scanSegments::segment_fills_region(Slic3r::LayerRegion* region)
{
    if (region->thin_fills.entities.empty()) return;
    for (const Slic3r::ExtrusionEntity* e : region->thin_fills.entities) {
        if (const Slic3r::ExtrusionPath* path = dynamic_cast<const Slic3r::ExtrusionPath*>(e)) {
            Marc::ScanSegmentPolyline seg;
            seg.polys = Slic3r::Polylines{ path->polyline };
            seg.type = Marc::CoreContourHatch;
            region->segment_polylines_.push_back(std::move(seg));
        }
    }
}

void scanSegments::segment_support_surfaces_region(Slic3r::LayerRegion* region)
{
    if (region->support_surfaces.surfaces.empty()) return;
    for (const Slic3r::Surface& s : region->support_surfaces.surfaces) {
        Marc::ScanSegmentPolyline seg;
        seg.polys = expolygon_to_polylines(s.expolygon);
        seg.type = Marc::ExternalSupports;
        region->segment_polylines_.push_back(std::move(seg));
    }
}

void scanSegments::segment_support_layer(const Slic3r::SupportLayer* layer)
{
    if (!layer) return;
    // Support islands as polygons ? ExternalSupports
    for (const Slic3r::ExPolygon& exp : layer->support_islands) {
        // For support layers, since they might not have regions, we need to handle differently.
        // Perhaps create a dummy region or find a way to store.
        // For now, since the user focused on LayerRegion, and support layers inherit from Layer,
        // if they have regions, populate them; otherwise, perhaps skip or handle separately.
        // But typically, support layers have their own structures.
        // To follow the instruction, if layer has regions, populate them.
        if (!layer->regions.empty()) {
            for (Slic3r::LayerRegion* region : layer->regions) {
                if (!region) continue;
                Marc::ScanSegmentPolyline seg;
                seg.polys = expolygon_to_polylines(exp);
                seg.type = Marc::ExternalSupports;
                region->segment_polylines_.push_back(std::move(seg));
            }
        }
        // If no regions, perhaps we need to create one or store elsewhere.
        // For simplicity, if no regions, skip for now.
    }
    for (const Slic3r::ExtrusionEntity* e : layer->support_fills.entities) {
        if (const Slic3r::ExtrusionPath* path = dynamic_cast<const Slic3r::ExtrusionPath*>(e)) {
            if (!layer->regions.empty()) {
                for (Slic3r::LayerRegion* region : layer->regions) {
                    if (!region) continue;
                    Marc::ScanSegmentPolyline seg;
                    seg.polys = Slic3r::Polylines{ path->polyline };
                    seg.type = Marc::ExternalSupports;
                    region->segment_polylines_.push_back(std::move(seg));
                }
            }
        }
    }
    for (const Slic3r::ExtrusionEntity* e : layer->support_interface_fills.entities) {
        if (const Slic3r::ExtrusionPath* path = dynamic_cast<const Slic3r::ExtrusionPath*>(e)) {
            if (!layer->regions.empty()) {
                for (Slic3r::LayerRegion* region : layer->regions) {
                    if (!region) continue;
                    Marc::ScanSegmentPolyline seg;
                    seg.polys = Slic3r::Polylines{ path->polyline };
                    seg.type = Marc::ExternalSupports;
                    region->segment_polylines_.push_back(std::move(seg));
                }
            }
        }
    }
}

Slic3r::Polylines scanSegments::polygon_to_polylines(const Slic3r::Polygon& poly)
{
    return Slic3r::Polylines{ poly.split_at_first_point() };
}

Slic3r::Polylines scanSegments::expolygon_to_polylines(const Slic3r::ExPolygon& exp)
{
    Slic3r::Polylines pls = polygon_to_polylines(exp.contour);
    for (const auto& hole : exp.holes) {
        Slic3r::Polylines hpl = polygon_to_polylines(hole);
        pls.insert(pls.end(), hpl.begin(), hpl.end());
    }
    return pls;
}

Marc::ThermalSegmentType scanSegments::role_to_segment(Slic3r::ExtrusionRole role)
{
    using namespace Slic3r;
    switch (role) {
        case erPerimeter:             return Marc::CoreContour_Volume;
        case erExternalPerimeter:     return Marc::CoreContour_Volume;
        case erOverhangPerimeter:     return Marc::CoreContour_Overhang;
        case erInternalInfill:        return Marc::CoreNormalHatch;
        case erSolidInfill:           return Marc::CoreContourHatch;
        case erTopSolidInfill:        return Marc::CoreContourHatch;
        case erBridgeInfill:          return Marc::CoreOverhangHatch;
        case erGapFill:               return Marc::CoreContourHatch;
        case erSkirt:                 return Marc::CoreContour_Volume;
        case erSupportMaterial:       return Marc::ExternalSupports;
        case erSupportMaterialInterface:return Marc::ExternalSupports;
        case erNone:                  return Marc::CoreContour_Volume;
        default:                      return Marc::CoreContour_Volume;
    }
}

} // namespace Marc
