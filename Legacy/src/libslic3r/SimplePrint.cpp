#include "SimplePrint.hpp"
#include "ClipperUtils.hpp"
#include <iostream>

namespace Slic3r {

void
SimplePrint::set_model(const Model &model) {
    this->_model = model;
    
    // make method idempotent so that the object is reusable
    this->_print.clear_objects();
    
    // align to z = 0
    for (ModelObject* o : this->_model.objects)
        o->translate(0, 0, -o->bounding_box().min.z);
    
    // make sure all objects have at least one defined instance
    if (this->_model.add_default_instances() && this->arrange) {
        // if we added at least one default instance, we need to rearrange
        //const BoundingBoxf bb;//{ this->_print.config.bed_shape.values };

          BoundingBoxf bb(Pointf(-50, -50), Pointf(50, 50));
        this->_model.arrange_objects(5, &bb);  // arrange at 5mm
    }
    
    if (this->center) {
        const Polygon bed_polygon = Polygon({
    Point(-50, -50),
    Point(50, -50),
    Point(50, 50),
    Point(-50, 50)
});
        //const Polygon bed_polygon{ scale(this->_print.config.bed_shape.values) };
        this->_model.center_instances_around_point(Slic3r::Pointf::new_unscale(bed_polygon.centroid()));
    }
    
    for (ModelObject* o : this->_model.objects) {
        this->_print.auto_assign_extruders(o);
        this->_print.add_model_object(o);
    }
}

/*void
SimplePrint::export_gcode(std::string outfile) {
    this->_print.status_cb = this->status_cb;
    this->_print.validate();
    //this->_print.export_gcode(outfile);
    
    // check that all parts fit in bed shape, and warn if they don't
    // TODO: use actual toolpaths instead of total bounding box
    Polygon bed_polygon{ scale(this->_print.config.bed_shape.values) };
    if (!diff(this->_print.bounding_box().polygon(), bed_polygon).empty()) {
        std::cout << "Warning: the supplied parts might not fit in the configured bed shape. "
            << "You might want to review the result before printing." << std::endl;
    }
    
    this->_print.status_cb = nullptr;
}*/

}
