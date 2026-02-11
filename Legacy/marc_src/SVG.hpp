#pragma once

//#include "../libslic3r/libslic3r.h"
#include "../clipper.hpp"
#include "../libslic3r/ExPolygon.hpp"
#include "../libslic3r/Line.hpp"
#include "../libslic3r/TriangleMesh.hpp"
#include "../libslic3r/Surface.hpp"
#include <filesystem>
namespace Marc {

class SVG
{
    public:
    bool arrows;
    std::string fill, stroke;
    bool flipY;
    std::string filename;
    Slic3r::Point origin;

    SVG(const char* afilename) :
    arrows(false), fill("grey"), stroke("black"), flipY(false), filename(afilename), origin()
    { open(filename); }

SVG(const char* afilename, const BoundingBox &bbox, const coord_t bbox_offset = scale_(1.), bool aflipY = false) : 
    arrows(false), fill("grey"), stroke("black"), flipY(aflipY), filename(afilename), origin(bbox.min - Slic3r::Point(bbox_offset, bbox_offset))
    { open(filename, bbox, bbox_offset, aflipY); }

SVG(const std::string &filename) :
    arrows(false), fill("grey"), stroke("black"), flipY(false), filename(filename), origin()
    { open(filename); }

SVG(const std::string &filename, const BoundingBox &bbox, const coord_t bbox_offset = scale_(1.), bool aflipY = false) : 
    arrows(false), fill("grey"), stroke("black"), flipY(aflipY), filename(filename), origin(bbox.min - Slic3r::Point(bbox_offset, bbox_offset))
    { open(filename, bbox, bbox_offset, aflipY); }

~SVG() { if (f != NULL) Close(); }

    bool open(const char* filename);
    bool open(const char* filename, const BoundingBox &bbox, const coord_t bbox_offset = scale_(1.), bool flipY = false);
    bool open(const std::string &filename) 
        { return open(filename.c_str()); }
    bool open(const std::string &filename, const BoundingBox &bbox, const coord_t bbox_offset = scale_(1.), bool flipY = false)
        { return open(filename.c_str(), bbox, bbox_offset, flipY); }

    void draw(const Slic3r::Line &line, std::string stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::ThickLine &line, const std::string &fill, const std::string &stroke, coordf_t stroke_width = 0);
    void draw(const Slic3r::Lines &lines, std::string stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::IntersectionLines &lines, std::string stroke = "black");

    void draw(const Slic3r::ExPolygon &expolygon, std::string fill = "grey", const float fill_opacity=1.f);
    void draw_outline(const Slic3r::ExPolygon &polygon, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);
    void draw(const Slic3r::ExPolygons &expolygons, std::string fill = "grey", const float fill_opacity=1.f);
    void draw_outline(const Slic3r::ExPolygons &polygons, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);

    void draw(const Slic3r::Surface &surface, std::string fill = "grey", const float fill_opacity=1.f);
    void draw_outline(const Slic3r::Surface &surface, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);
    void draw(const Slic3r::Surfaces &surfaces, std::string fill = "grey", const float fill_opacity=1.f);
    void draw_outline(const Slic3r::Surfaces &surfaces, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);
    void draw(const Slic3r::SurfacesPtr &surfaces, std::string fill = "grey", const float fill_opacity=1.f);
    void draw_outline(const Slic3r::SurfacesPtr &surfaces, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);
 
    void draw(const Slic3r::Polygon &polygon, std::string fill = "grey");
    void draw_outline(const Polygon &polygon, std::string stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::Polygons &polygons, std::string fill = "grey");
    void draw_outline(const Slic3r::Polygons &polygons, std::string stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::Polyline &polyline, std::string stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::Polylines &polylines, std::string stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::ThickLines &thicklines, const std::string &fill = "lime", const std::string &stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::ThickPolylines &polylines, const std::string &stroke = "black", coordf_t stroke_width = 0);
    void draw(const Slic3r::ThickPolylines &thickpolylines, const std::string &fill, const std::string &stroke, coordf_t stroke_width);
    void draw(const Slic3r::Point &point, std::string fill = "black", coord_t radius = 0);
    void draw(const Slic3r::Points &points, std::string fill = "black", coord_t radius = 0);

    // Support for rendering the ClipperLib paths
    void draw(const ClipperLib::Path  &polygon, double scale, std::string fill = "grey", coordf_t stroke_width = 0);
    void draw(const ClipperLib::Paths &polygons, double scale, std::string fill = "grey", coordf_t stroke_width = 0);

    void draw_text(const Point &pt, const char *text, const char *color);
    void draw_legend(const Point &pt, const char *text, const char *color);
    void layer_section_start(const char *text);
	void layer_section_end();
    void Close();
    
    private:
    //std::string filename;
    FILE* f;
    
    void path(const std::string &d, bool fill, coordf_t stroke_width, const float fill_opacity);
    std::string get_path_d(const Slic3r::MultiPoint &mp, bool closed = false) const;
    std::string get_path_d(const ClipperLib::Path &mp, double scale, bool closed = false) const;

public:
    static void export_expolygons(const char *path, const Slic3r::BoundingBox &bbox, const Slic3r::ExPolygons &expolygons, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);
    static void export_expolygons(const std::string &path, const Slic3r::BoundingBox &bbox, const Slic3r::ExPolygons &expolygons, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0) 
        { export_expolygons(path.c_str(), bbox, expolygons, stroke_outer, stroke_holes, stroke_width); }
};

}


