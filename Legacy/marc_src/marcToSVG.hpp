#pragma once

#include "../libslic3r/libslic3r.h"
#include "../clipper.hpp"
//#include "../libslic3r/ExPolygon.hpp"
//#include "../libslic3r/Line.hpp"
//#include "../libslic3r/TriangleMesh.hpp"
//#include "../libslic3r/Surface.hpp"
#include <filesystem>
#include "ExportMarcSlice.hpp"
namespace Marc {

class marcToSVG
{
    public:
    bool arrows;
    std::string fill, stroke;
    bool flipY;
    std::string filename;
    Point origin;

    marcToSVG(const char* afilename) :
    arrows(false), fill("grey"), stroke("black"), flipY(false), filename(afilename), origin()
    { open(filename); }



~marcToSVG() { if (f != NULL) Close(); }

    bool open(const char* filename);
     bool open(const std::string &filename) 
        { return open(filename.c_str()); }
   
    void draw(const Line &line, std::string stroke = "green", float stroke_width = 0.1);

    void draw(const std::vector<Line>  &lines, std::string stroke = "green", float stroke_width = 0.1);
  
    void draw(const Polygon &polygon, std::string stroke = "red", float stroke_width = 0.1);
    void draw(const std::vector<Polygon> &polygons, std::string stroke = "red", float stroke_width = 0.1);
    
    void draw(const Polyline &polyline, std::string stroke = "black", float stroke_width = 0.1);
    void draw(const std::vector<Polyline> &polylines, std::string stroke = "black", float stroke_width = 0.1);

   

    void draw(const Point &point, std::string fill = "black", float radius = 0);
    void draw(const std::vector<Point> &points, std::string fill = "black", float radius = 0);

    // Support for rendering the ClipperLib paths
 

    void draw_text(const Point &pt, const char *text, const char *color);
    void draw_legend(const Point &pt, const char *text, const char *color);
    void layer_section_start(const char *text);
	void layer_section_end();
    void Close();
    
    private:
    //std::string filename;
    FILE* f;
    
  void path(const std::string &d, bool fill, float stroke_width, const float fill_opacity);
    std::string get_path_d(const std::vector<Point> &mp, bool closed = false) const;
    //std::string get_path_d(const ClipperLib::Path &mp, double scale, bool closed = false) const;

public:
    //static void export_expolygons(const char *path, const Slic3r::BoundingBox &bbox, const Slic3r::ExPolygons &expolygons, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0);
    //static void export_expolygons(const std::string &path, const Slic3r::BoundingBox &bbox, const Slic3r::ExPolygons &expolygons, std::string stroke_outer = "black", std::string stroke_holes = "blue", coordf_t stroke_width = 0) 
        //{ export_expolygons(path.c_str(), bbox, expolygons, stroke_outer, stroke_holes, stroke_width); }
};

}


