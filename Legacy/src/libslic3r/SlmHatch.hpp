#ifndef slic3r_SlmHatch_hpp_
#define slic3r_SlmHatch_hpp_

#include "libslic3r.h"
#include "Line.hpp"
#include "MultiPoint.hpp"
#include "Polygon.hpp"
#include <string>
#include <vector>
#include "clipper.hpp"
#include "Polyline.hpp"

#include "ExPolygon.hpp"
#include "ExPolygonCollection.hpp"
#include "Line.hpp"

#include "ClipperUtils.hpp"
#include <iostream>
#include <utility>
#include "libslic3r.h"
#include "ClipperUtils.hpp"

namespace Slic3r {


//typedef std::vector<SlmHatch> SlmHatches;


class SlmHatch 
{
    public:
    Lines hatch_lines;
    SurfaceType surface_type;
    

    SlmHatch(Lines h_lines, SurfaceType s_type) : hatch_lines(std::move(h_lines)), surface_type(s_type) {}
    ~SlmHatch() = default;
    Lines lines() const;


};




}

#endif
