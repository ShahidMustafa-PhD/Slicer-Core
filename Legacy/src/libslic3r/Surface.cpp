#include "Surface.hpp"

namespace Slic3r {

Surface::operator Polygons() const
{
    return this->expolygon;
}

double
Surface::area() const
{
    return this->expolygon.area();
}

bool
Surface::is_solid() const
{
    return (this->surface_type & (stTop | stBottom | stSolid | stBridge)) != 0;
}

bool
Surface::is_external() const
{
    return is_top() || is_bottom();
}

bool
Surface::is_internal() const
{
    return (this->surface_type & stInternal) != 0;
}

bool
Surface::is_bottom() const
{
    return (this->surface_type & stBottom) != 0;
}

bool
Surface::is_top() const
{
    return (this->surface_type & stTop) != 0;
}

bool
Surface::is_bridge() const
{
    return (this->surface_type & stBridge) != 0;
}

Polygons
Surface::RectGrids(float island_L,float island_H) const
{   
    BoundingBox R=this->expolygon.bounding_box();
    Surfaces  Grid_surfaces;
	//SurfaceCollection Grid_surfaces;//::append(const ExPolygons &src, SurfaceType surfaceType)
	Point size = R.size();
       // this->size = Point3(scale_(size.x), scale_(size.y), scale_(size.z));
	int correctL= std::ceil(size.x/scale_(island_L));
	int correctH= std::ceil(size.y/scale_(island_H));
	if (correctL==0)   correctL=1;
	if (correctH==0)   correctH=1;
	coord_t island_LInt=size.x/correctL;
	coord_t island_HInt=size.y/correctH;
	//coord_t island_HInt=scale_(this->config.island_H.value);
   // coord_t island_LInt=scale_(this->config.island_L.value);
	
	
	
	Polygons rectangularGrids;
	
	for(coord_t ycc=R.min.y; ycc < R.max.y; ycc+=island_HInt)
    {  ycc+=20;//scale_(this->config.SLM_overlap.value);//HatchDistanInt;
        		
		for(coord_t xcc=R.min.x; xcc < R.max.x; xcc+=island_LInt)
        {  xcc+=20;//scale_(this->config.SLM_overlap.value);//HatchDistanInt;
	        Points gridp;
			gridp.push_back(Point(xcc,ycc));
	        gridp.push_back(Point(xcc,ycc+island_HInt));
	        gridp.push_back(Point(xcc+island_LInt,ycc+island_HInt));
            gridp.push_back(Point(xcc+island_LInt,ycc));
		    rectangularGrids.push_back(Polygon(gridp));
 
			
        }
    }
	    
		//ExPolygons pp= intersection_ex(rectangularGrids, Polygons(this->expolygon));
	
		//intersection_ex(const Slic3r::Polygons &subject, const Slic3r::Polygons &clip, bool safety_offset_ = false)
//{if(this->expolygon.holes.empty())
	    //Grid_surfaces.append(pp,this->surface_type);
	   //else 
	 //  Grid_surfaces.append(diff_ex(pp,this->expolygon.holes),this->surface_type);
	

return   rectangularGrids;//  intersection_ex(rectangularGrids, Polygons(this->expolygon));
  
  }





}
