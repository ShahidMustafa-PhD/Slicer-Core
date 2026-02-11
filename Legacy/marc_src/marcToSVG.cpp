#include "marcToSVG.hpp"
#include <iostream>

#define COORD(x) (x*10)   //  scaled to 0.1mm.

namespace Marc {

bool marcToSVG::open(const char* afilename)
{
    this->filename = afilename;
    this->origin.x = -100;
    this->origin.y = -100;
    this->f = fopen(afilename, "w");
    if (this->f == NULL) {
        return false;
    }
    fprintf(this->f,
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\" \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n"
        "<svg height=\"2000\" width=\"2000\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">\n"
        "   <marker id=\"endArrow\" markerHeight=\"8\" markerUnits=\"strokeWidth\" markerWidth=\"10\" orient=\"auto\" refX=\"1\" refY=\"5\" viewBox=\"0 0 1000 1000\">\n"
        "      <polyline fill=\"darkblue\" points=\"0,0 10,5 0,10 1,5\" />\n"
        "   </marker>\n"
        );
    
        
        
    return true;
}

void
marcToSVG::draw(const Line &line, std::string stroke, float stroke_width)
{
    fprintf(this->f,
        "   <line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" style=\"stroke: %s; stroke-width: %f\"",
        COORD(line.a.x - origin.x), COORD(line.a.y - origin.y), COORD(line.b.x - origin.x), COORD(line.b.y - origin.y), stroke.c_str(), (stroke_width == 0) ? 1.f : 0.2);//COORD(stroke_width*100000));
    if (this->arrows)
        fprintf(this->f, " marker-end=\"url(#endArrow)\"");
    fprintf(this->f, "/>\n");
}



void
marcToSVG::draw(const std::vector<Line>  &lines, std::string stroke, float stroke_width)
{
    for (std::vector<Line>::const_iterator it = lines.begin(); it != lines.end(); ++it)
        this->draw(*it, stroke, stroke_width);
}





void
marcToSVG::draw(const Polyline &polyline, std::string stroke, float stroke_width)
{
     //std::cout << "Draw Polyline:\n "<<std::endl;
	this->stroke = stroke;
    this->path(this->get_path_d(polyline.points, false), false, stroke_width, 1.f);
}

void
marcToSVG::draw(const std::vector<Polyline> &polylines, std::string stroke, float stroke_width)
{    //std::cout << "Draw Polyline:\n "<<std::endl;
    for (std::vector<Polyline>::const_iterator it = polylines.begin(); it != polylines.end(); ++it)
        this->draw(*it, stroke, stroke_width);
}


void 
marcToSVG::draw(const Polygon &polygon, std::string stroke, float stroke_width)
{
    this->stroke = stroke;
    this->path(this->get_path_d(polygon.points, true), false, stroke_width, 1.f);
}
    
void marcToSVG::draw(const std::vector<Polygon> &polygons, std::string stroke, float stroke_width)
{   for (std::vector<Polygon>::const_iterator it = polygons.begin(); it != polygons.end(); ++it)
        this->draw(*it, stroke, stroke_width);
 }




void
marcToSVG::draw(const Point &point, std::string fill, float iradius)
{
    float radius = (iradius == 0) ? 3.f : COORD(iradius);  //  scaled up
    std::ostringstream svg;
    svg << "   <circle cx=\"" << COORD(point.x - origin.x) <<  "\" cy=\"" << COORD(point.y - origin.y)
        << "\" r=\"" << radius << "\" "
        << "style=\"stroke:black; fill: " << fill << "\" />";
    
    fprintf(this->f, "%s\n", svg.str().c_str());
}

void
marcToSVG::draw(const std::vector<Point> &points, std::string fill, float radius)
{
    for (std::vector<Point>::const_iterator it = points.begin(); it != points.end(); ++it)
        this->draw(*it, fill, radius);
}






void
marcToSVG::path(const std::string &d, bool fill, float stroke_width, const float fill_opacity)
{
    float lineWidth = 0.f;
    if (! fill)
        lineWidth = (stroke_width == 0) ? 2.f : stroke_width;

    fprintf(
        this->f,
        "   <path d=\"%s\" style=\"fill: %s; stroke: %s; stroke-width: %f; fill-type: evenodd\" %s fill-opacity=\"%f\" />\n",
        d.c_str(),
        fill ? this->fill.c_str() : "none",
        this->stroke.c_str(),
        lineWidth, 
        (this->arrows && !fill) ? " marker-end=\"url(#endArrow)\"" : "",
        fill_opacity
    );
}

std::string
marcToSVG::get_path_d(const std::vector<Point> &mp, bool closed) const
{
    std::ostringstream d;
    d << "M ";
    for (std::vector<Point>::const_iterator p = mp.begin(); p != mp.end(); ++p) {
        d << COORD(p->x - origin.x) << "  ";
        d << COORD(p->y - origin.y) << "  ";
    }
    if (closed) d << "Z";
    return d.str();
}



void marcToSVG::draw_text(const Point &pt, const char *text, const char *color)
{
    fprintf(this->f,
        "<text x=\"%f\" y=\"%f\" font-family=\"sans-serif\" font-size=\"20px\" fill=\"%s\">%s</text>",
        COORD(pt.x-origin.x),
        COORD(pt.y-origin.y),
        color, text);
}

void marcToSVG::draw_legend(const Point &pt, const char *text, const char *color)
{
    fprintf(this->f,
        "<circle cx=\"%f\" cy=\"%f\" r=\"10\" fill=\"%s\"/>",
        COORD(pt.x-origin.x),
        COORD(pt.y-origin.y),
        color);
    fprintf(this->f,
        "<text x=\"%f\" y=\"%f\" font-family=\"sans-serif\" font-size=\"10px\" fill=\"%s\">%s</text>",
        COORD(pt.x-origin.x) + 20.f,
        COORD(pt.y-origin.y),
        "black", text);
}
 void marcToSVG::layer_section_start(const char *text)
{//fprintf(this->f, <g id="layer0" slic3r:z="1.75e-007">
  //<g id="layer0" slic3r:z="1.75e-007">
 fprintf(this->f,
        "<g id=\"%s\">\n", text);
 }
  void marcToSVG::layer_section_end()
{//fprintf(this->f, <g id="layer0" slic3r:z="1.75e-007">
  //<g id="layer0" slic3r:z="1.75e-007">
 fprintf(this->f,
        "</g >\n");
 }
void
marcToSVG::Close()
{
    fprintf(this->f, "</svg>\n");
    fclose(this->f);
    this->f = NULL;
//    printf("SVG written to %s\n", this->filename.c_str());
}



}
