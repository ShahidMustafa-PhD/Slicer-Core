#ifndef slic3r_SLMCONFIG_hpp_
#define slic3r_SLMCONFIG_hpp_

#include <string>
namespace Slic3r {

#include <string>

struct SlmConfig {
    // CONSTANT
    double beam_diameter = 0.1;
    double layer_thickness = 0.2;
    double first_layer_thickness = 0.3;
    double hatch_spacing=0.1;
    double hatch_angle = 45; // 0.0 for horizontal, 90.0 for vertical
   // double meltpool_width = 0.5;  // for extrusion_width
   double island_width=5;
    double island_height=5;
    int threads = 12;
    double z_steps_per_mm=1000;
    double anchors=7; // anchors height in mm
     int perimeters = 3;
   
    double anchors_layer_thickness=0.5; // anchors layer thickness in mm
    double xy_size_compensation = 0.1; // xy size compensation in mm
    double regions_overlap = 0.1; // overlap between regions in mm
   
    double overhangs_angle = 45; // angle for overhangs in degrees
    bool external_perimeters_first = true; // print external perimeters first;
    // SUPPORT SETTINGS
   
    //double perimeter_extrusion_width = 0.09;
    double perimeter_hatch_spacing = 0.09;
    double support_material_spacing = 2.5;
    double support_material_angle = 45;
   
    double support_material_model_clearance = 1.5;
     double support_material_pillar_size = 1.2;
    double support_material_pillar_spacing = 3.0;
    //bool support_material_buildplate_only = false;
    //int support_material_interface_layers = 4;
   // double support_material_interface_spacing = 3.0;
   
   
    //double external_perimeter_extrusion_width = 0.09;
   // double top_infill_extrusion_width = 0.09;
    //double infill_extrusion_width = 0.09;
   // double solid_infill_extrusion_width = 0.09;
    //double support_material_extrusion_width = 0.09;
    //double support_material_interface_extrusion_width = 0.09;

    // DISABLED
    //bool avoid_crossing_perimeters = true;
    //bool only_retract_when_crossing_perimeters = true;
    //double extrusion_multiplier = 1.0;
    //int infill_acceleration = 1;
    //int infill_extruder = 1;
   // int solid_infill_extruder = 1;
    //double solid_infill_below_area = 10.0;
    
    // FILL SETTINGS
    
    //std::string fill_pattern = "triangular";
    //std::string bottom_infill_pattern = "gyroid";
    //std::string top_infill_pattern = "gyroid";
    //int infill_overlap = 10;
   // bool infill_only_where_needed = false;
    
    //int infill_every_layers = 1;
    //int solid_infill_every_layers = 1;
    // SHELL SETTINGS
   
    //double min_shell_thickness = 0.16;
    //double min_top_bottom_shell_thickness = 2.0;
    //int top_solid_layers = 8;
    //int bottom_solid_layers = 8;
    
    // SLICER SETTINGS
    // double nozzle_diameter = 0.2;
    
    // double beam_size = 0.2;
    // double xy_size_compensation = 0.09;
   
    // double resolution = 0.005;
    // bool adaptive_slicing = false;
    // int adaptive_slicing_quality = 75;
  
 
    //std::string support_material_pattern = "gyroid";
 
    //int support_material_interface_extruder = 1;
    //int support_material_extruder = 1;
    //double support_material_speed = 60.0;
    bool adaptive_slicing=0;
    int fill_density = 100;
    bool complete_objects = true;
    double duplicate_distance = 6.0;
    bool fill_gaps = true;
    bool extra_perimeters = true;
    bool thin_walls = true;
    bool infill_first = true;
    bool overhangs = true; // enable overhangs
    bool support_material = true;
    bool match_horizontal_surfaces = true;
    bool interface_shells = true;
    int support_material_max_layers = 10;
    int support_material_enforce_layers = 10;
     double support_material_threshold = 45;
};



};



#endif
