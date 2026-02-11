#pragma once
#include <vector>

#include "../libslic3r/Print.hpp"         // Your print object structure
#include "../libslic3r/libslic3r.h"
#include "../libslic3r/Flow.hpp"
#include "../libslic3r/SurfaceCollection.hpp"
#include "../libslic3r/ExtrusionEntityCollection.hpp"
#include "../libslic3r/ExPolygonCollection.hpp"
#include "../libslic3r/PolylineCollection.hpp"
//#include <boost/thread.hpp>
//#include "../libslic3r/PrintConfig.hpp"
#include "../libslic3r/SurfaceCollection.hpp"
#include "../libslic3r/Line.hpp"
#include "SVG.hpp"
namespace Marc {
     

    
class slmHatches {
public:
    // Constructor
    slmHatches(PrintObject* printObject, float infillDensity);

    // Method to generate hatches
    void generateHatches();

    // Method to generate infill hatches
    void generateInfillHatches();

    // Method to generate perimeter hatches
    void generatePerimeterHatches();

    // Get the generated hatches
    const std::vector<Polygon>& getHatches() const;
    std::vector<Polygon> generateHatchPathsForPolygon(const Polygon& polygon, float hatchSpacing);
    void fill_hatches();
private:
    // Helper method to calculate hatch spacing based on infill density
    float calculateHatchSpacing(float infillDensity) const;

    // Data members
    PrintObject* printObject; // Pointer to the print object
    float infillDensity;

};

}
