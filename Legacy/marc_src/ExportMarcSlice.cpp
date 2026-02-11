#include "ExportMarcSlice.hpp"
#include <iostream>
#include "../libslic3r/Surface.hpp"
#define COORD(x) (static_cast<float>(Slic3r::unscale((x))))   //  scaled to 1mm.

namespace Marc{

// Constructors are now in the header with default implementation

void
ExportMarcSlice::convrt(const Slic3r::Line &line, Line &m_line) const
{
    Point a;
    Point b;
     a.x=COORD(line.a.x )- origin_.x;  // Access via private member
     a.y=COORD(line.a.y )- origin_.y;
     b.x=COORD(line.b.x )- origin_.x;
     b.y=COORD(line.b.y) - origin_.y;
    m_line.a=a;
    m_line.b=b;
}


void ExportMarcSlice::convrt(const Slic3r::Lines& lines, std::vector<Line>& m_lines) const
{
    for (const auto& line : lines)
    {
        Line m_line;
        this->convrt(line, m_line);
        m_lines.emplace_back(std::move(m_line)); // More efficient
    }
}


void ExportMarcSlice::convrt(const Slic3r::ExPolygon& expolygon, std::vector<Polygon>& m_polygons) const
{
    Polygon contour_polygon;
    convrt(expolygon.contour, contour_polygon);
    m_polygons.emplace_back(std::move(contour_polygon)); // More efficient
    for (const auto& hole : expolygon.holes)
    {
        Polygon hole_polygon;
        convrt(hole, hole_polygon);
        m_polygons.emplace_back(std::move(hole_polygon)); // More efficient
    }
}

void ExportMarcSlice::convrt(const Slic3r::ExPolygons& expolygons, std::vector<Polygon>& m_polygons) const
{
    for (const auto& expolygon : expolygons)
    {
        convrt(expolygon, m_polygons);
    }
}

void ExportMarcSlice::convrt(const Slic3r::Polygon& polygon, Polygon& m_polygon) const
{
    std::vector<Point> m_points;
    for (const auto& p : polygon.points)
    {
        Point m_point;
        m_point.x = COORD(p.x)-origin_.x;
        m_point.y = COORD(p.y)-origin_.y;
        m_points.push_back(m_point);
    }
    m_polygon.points = std::move(m_points);
}



void ExportMarcSlice::convrt(const Slic3r::Polyline& polyline, Polyline& m_polyline) const
{
    std::vector<Point> m_points;
    for (const auto& p : polyline.points)
    {
        Point m_point;
        m_point.x = COORD(p.x )-origin_.x;
        m_point.y = COORD(p.y )-origin_.y;
        m_points.push_back(m_point);
    }
    m_polyline.points = std::move(m_points);
}
    



void ExportMarcSlice::convrt(const Slic3r::Polylines& polylines, std::vector<Polyline>& m_polylines) const
{
    for (const auto& polyline : polylines)
    {
        Polyline m_polyline;
        convrt(polyline, m_polyline);
        m_polylines.emplace_back(std::move(m_polyline)); // More efficient
    }
}



void ExportMarcSlice::convrt(const Slic3r::Point& point, Point& m_point) const
{
    m_point.x = COORD(point.x )-origin_.x;
    m_point.y = COORD(point.y )-origin_.y;
}

void ExportMarcSlice::convrt(const Slic3r::Points& points, std::vector<Point>& m_points) const
{
    for (const auto& point : points)
    {
        Point m_point;
        this->convrt(point, m_point);
        m_points.emplace_back(std::move(m_point)); // More efficient
    }
}


void ExportMarcSlice::convrt(const Slic3r::MultiPoint& mp, Polyline& m_polyline) const
{
    std::vector<Point> m_points;
    for (const auto& p : mp.points)
    {
        Point m_point;
        m_point.x = COORD(p.x )-origin_.x;
        m_point.y = COORD(p.y )-origin_.y;
        m_points.push_back(m_point);
    }
    m_polyline.points = std::move(m_points);
}

void ExportMarcSlice::convrt(const Slic3r::Polygons& polygons, std::vector<Polygon>& m_polygons) const
{
    for (const auto& polygon : polygons)
    {
        Polygon m_polygon;
        convrt(polygon, m_polygon);  // This calls the existing draw function for Polygon
        m_polygons.emplace_back(std::move(m_polygon)); // More efficient
    }
}

void
ExportMarcSlice::convrt(const Slic3r::Surface& surface,std::vector<Polygon>& m_polygons) const
{
    convrt(surface.expolygon,m_polygons);
}



void
ExportMarcSlice::convrt(const Slic3r::Surfaces& surfaces, std::vector<Polygon>& m_polygons) const
{
    for (const auto& surface : surfaces)
    {
        convrt(surface, m_polygons);
    }
}








void
ExportMarcSlice::Close()
{
 
}

} // namespace Marc

