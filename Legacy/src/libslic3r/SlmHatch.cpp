
#include "SlmHatch.hpp"
namespace Slic3r {

Lines
SlmHatch::lines() const
{
    return this->hatch_lines;
}



/*Lines SlmHatch::hatch_lines(double island_L, double island_H, double overlap_mm = 0.1)
{
    BoundingBox bounding_box= this->surface.expolygon.bounding_box();
    // Early exit for invalid dimensions
    Lines hatch_lines;
    if (island_L <= 0 || island_H <= 0) return hatch_lines;

    // Clear any existing grid data
    //rectangularGrids.clear();
    Polygons rectangularGrids;
    // Convert dimensions to internal scaling
    coord_t island_LInt = scale_(island_L);
    coord_t island_HInt = scale_(island_H);
    coord_t overlapInt = scale_(overlap_mm);
    
    // Create grid cells
    for (coord_t y = bounding_box.min.y; y < bounding_box.max.y; y += (island_HInt - overlapInt)) {
        for (coord_t x = bounding_box.min.x; x < bounding_box.max.x; x += (island_LInt - overlapInt)) {
            // Create a rectangle as a polygon with 4 points
            Points rectangle;
            rectangle.reserve(4);
            
            rectangle.push_back(Point(x, y));
            rectangle.push_back(Point(x, y + island_HInt));
            rectangle.push_back(Point(x + island_LInt, y + island_HInt));
            rectangle.push_back(Point(x + island_LInt, y));
            
            rectangularGrids.push_back(Polygon(rectangle));
        }
    }

    


    return hatch_lines;
}*/
}
