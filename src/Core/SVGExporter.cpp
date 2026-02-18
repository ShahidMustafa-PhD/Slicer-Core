// ==============================================================================
// MarcSLM - SVG Exporter Implementation
// ==============================================================================
// Ported from Legacy marcToSVG.cpp
// ==============================================================================

#include "MarcSLM/Core/SVGExporter.hpp"

#include <iostream>
#include <cmath>

#define SVG_COORD(x) ((x) * 10.0f)  // Scale to 0.1mm per SVG pixel

namespace MarcSLM {

SVGExporter::SVGExporter(const char* afilename) {
    origin.x = -100.0f;
    origin.y = -100.0f;
    filename = afilename;
    open(afilename);
}

SVGExporter::~SVGExporter() {
    if (f != nullptr) Close();
}

bool SVGExporter::open(const char* afilename) {
    filename = afilename;
    f = fopen(afilename, "w");
    if (f == nullptr) return false;

    fprintf(f,
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\" "
        "\"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"
        "<svg height=\"2000\" width=\"2000\" "
        "xmlns=\"http://www.w3.org/2000/svg\" "
        "xmlns:svg=\"http://www.w3.org/2000/svg\" "
        "xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
        "  <marker id=\"endArrow\" markerHeight=\"8\" markerUnits=\"strokeWidth\" "
        "markerWidth=\"10\" orient=\"auto\" refX=\"1\" refY=\"5\" "
        "viewBox=\"0 0 1000 1000\">\n"
        "    <polyline fill=\"darkblue\" points=\"0,0 10,5 0,10 1,5\" />\n"
        "  </marker>\n");

    return true;
}

void SVGExporter::draw(const Marc::Line& line,
                       const std::string& color, float stroke_width) {
    if (!f) return;
    fprintf(f,
        "  <line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" "
        "style=\"stroke: %s; stroke-width: %f\"",
        SVG_COORD(line.a.x - origin.x),
        SVG_COORD(line.a.y - origin.y),
        SVG_COORD(line.b.x - origin.x),
        SVG_COORD(line.b.y - origin.y),
        color.c_str(),
        (stroke_width == 0.0f) ? 1.0f : 0.2f);
    if (arrows) fprintf(f, " marker-end=\"url(#endArrow)\"");
    fprintf(f, "/>\n");
}

void SVGExporter::draw(const std::vector<Marc::Line>& lines,
                       const std::string& color, float stroke_width) {
    for (const auto& line : lines) draw(line, color, stroke_width);
}

void SVGExporter::draw(const Marc::Polyline& polyline,
                       const std::string& color, float stroke_width) {
    if (!f) return;
    stroke = color;
    path(get_path_d(polyline.points, false), false, stroke_width, 1.0f);
}

void SVGExporter::draw(const std::vector<Marc::Polyline>& polylines,
                       const std::string& color, float stroke_width) {
    for (const auto& pl : polylines) draw(pl, color, stroke_width);
}

void SVGExporter::draw(const Marc::Polygon& polygon,
                       const std::string& color, float stroke_width) {
    if (!f) return;
    stroke = color;
    path(get_path_d(polygon.points, true), false, stroke_width, 1.0f);
}

void SVGExporter::draw(const std::vector<Marc::Polygon>& polygons,
                       const std::string& color, float stroke_width) {
    for (const auto& pg : polygons) draw(pg, color, stroke_width);
}

void SVGExporter::draw(const Marc::Point& point,
                       const std::string& fillColor, float iradius) {
    if (!f) return;
    float radius = (iradius == 0.0f) ? 3.0f : SVG_COORD(iradius);
    fprintf(f,
        "  <circle cx=\"%f\" cy=\"%f\" r=\"%f\" "
        "style=\"stroke:black; fill: %s\" />\n",
        SVG_COORD(point.x - origin.x),
        SVG_COORD(point.y - origin.y),
        radius,
        fillColor.c_str());
}

void SVGExporter::draw(const std::vector<Marc::Point>& points,
                       const std::string& fillColor, float radius) {
    for (const auto& pt : points) draw(pt, fillColor, radius);
}

void SVGExporter::draw_text(const Marc::Point& pt, const char* text,
                            const char* color) {
    if (!f) return;
    fprintf(f,
        "<text x=\"%f\" y=\"%f\" font-family=\"sans-serif\" "
        "font-size=\"20px\" fill=\"%s\">%s</text>\n",
        SVG_COORD(pt.x - origin.x),
        SVG_COORD(pt.y - origin.y),
        color, text);
}

void SVGExporter::draw_legend(const Marc::Point& pt, const char* text,
                              const char* color) {
    if (!f) return;
    fprintf(f,
        "<circle cx=\"%f\" cy=\"%f\" r=\"10\" fill=\"%s\"/>\n",
        SVG_COORD(pt.x - origin.x),
        SVG_COORD(pt.y - origin.y),
        color);
    fprintf(f,
        "<text x=\"%f\" y=\"%f\" font-family=\"sans-serif\" "
        "font-size=\"10px\" fill=\"black\">%s</text>\n",
        SVG_COORD(pt.x - origin.x) + 20.0f,
        SVG_COORD(pt.y - origin.y),
        text);
}

void SVGExporter::layer_section_start(const char* text) {
    if (!f) return;
    fprintf(f, "<g id=\"%s\">\n", text);
}

void SVGExporter::layer_section_end() {
    if (!f) return;
    fprintf(f, "</g>\n");
}

void SVGExporter::Close() {
    if (!f) return;
    fprintf(f, "</svg>\n");
    fclose(f);
    f = nullptr;
}

void SVGExporter::path(const std::string& d, bool filled, float stroke_width,
                       float fill_opacity) {
    if (!f) return;
    float lineWidth = filled ? 0.0f : ((stroke_width == 0.0f) ? 2.0f : stroke_width);
    fprintf(f,
        "  <path d=\"%s\" style=\"fill: %s; stroke: %s; stroke-width: %f; "
        "fill-type: evenodd\" %s fill-opacity=\"%f\" />\n",
        d.c_str(),
        filled ? fill.c_str() : "none",
        stroke.c_str(),
        lineWidth,
        (arrows && !filled) ? " marker-end=\"url(#endArrow)\"" : "",
        fill_opacity);
}

std::string SVGExporter::get_path_d(const std::vector<Marc::Point>& pts,
                                     bool closed) const {
    std::ostringstream d;
    d << "M ";
    for (const auto& p : pts) {
        d << SVG_COORD(p.x - origin.x) << "  ";
        d << SVG_COORD(p.y - origin.y) << "  ";
    }
    if (closed) d << "Z";
    return d.str();
}

} // namespace MarcSLM
