// ==============================================================================
// MarcSLM - SLM Build Configuration
// ==============================================================================
// Ported from Legacy Slic3r-based SlmConfig.hpp
// All SLM/DMLS process parameters for the slicing engine
// ==============================================================================

#pragma once

#include <string>
#include <cstdint>

namespace MarcSLM {

/// @brief Complete SLM/DMLS process configuration.
/// @details Contains all parameters needed for slicing, hatching, perimeter
///          generation, support generation, and overhang detection.
///          Values are in millimeters and degrees unless noted otherwise.
///          Loaded from JSON configuration files at runtime.
struct SlmConfig {
    // ==============================================================================
    // Laser / Beam Parameters
    // ==============================================================================
    double beam_diameter = 0.1;              ///< Laser beam spot diameter [mm]

    // ==============================================================================
    // Layer Parameters
    // ==============================================================================
    double layer_thickness = 0.03;           ///< Nominal layer thickness [mm]
    double first_layer_thickness = 0.05;     ///< First layer thickness [mm]

    // ==============================================================================
    // Hatch Parameters
    // ==============================================================================
    double hatch_spacing = 0.1;              ///< Hatch line spacing [mm]
    double hatch_angle = 45.0;               ///< Hatch rotation angle [degrees]
    double island_width = 5.0;               ///< Island/checkerboard width [mm]
    double island_height = 5.0;              ///< Island/checkerboard height [mm]

    // ==============================================================================
    // Perimeter Parameters
    // ==============================================================================
    int    perimeters = 3;                   ///< Number of perimeter contour loops
    double perimeter_hatch_spacing = 0.09;   ///< Perimeter hatch spacing [mm]
    bool   external_perimeters_first = true; ///< Print external perimeters first

    // ==============================================================================
    // Processing Parameters
    // ==============================================================================
    int    threads = 12;                     ///< Number of parallel threads
    double z_steps_per_mm = 1000.0;          ///< Z-axis steps per mm (machine resolution)

    // ==============================================================================
    // Geometry Compensation
    // ==============================================================================
    double xy_size_compensation = 0.1;       ///< XY size compensation offset [mm]
    double regions_overlap = 0.1;            ///< Overlap between hatching regions [mm]

    // ==============================================================================
    // Overhang Detection
    // ==============================================================================
    double overhangs_angle = 45.0;           ///< Critical overhang angle [degrees]
    bool   overhangs = true;                 ///< Enable overhang detection

    // ==============================================================================
    // Anchor / Base Parameters
    // ==============================================================================
    double anchors = 7.0;                    ///< Anchor height [mm]
    double anchors_layer_thickness = 0.5;    ///< Anchor layer thickness [mm]

    // ==============================================================================
    // Support Parameters
    // ==============================================================================
    bool   support_material = true;          ///< Enable support material generation
    double support_material_spacing = 2.5;   ///< Support hatch spacing [mm]
    double support_material_angle = 45.0;    ///< Support hatch angle [degrees]
    double support_material_model_clearance = 1.5; ///< Gap between support and model [mm]
    double support_material_pillar_size = 1.2;     ///< Support pillar diameter [mm]
    double support_material_pillar_spacing = 3.0;  ///< Support pillar spacing [mm]
    double support_material_threshold = 45.0;      ///< Angle threshold for support [degrees]
    int    support_material_max_layers = 10;       ///< Max support layers
    int    support_material_enforce_layers = 10;   ///< Enforce support for N bottom layers

    // ==============================================================================
    // Fill / Infill Parameters
    // ==============================================================================
    int    fill_density = 100;               ///< Fill density percentage [0-100]
    bool   fill_gaps = true;                 ///< Fill small gaps between perimeters
    bool   infill_first = true;              ///< Generate infill before perimeters
    bool   extra_perimeters = true;          ///< Generate extra perimeters where needed
    bool   thin_walls = true;               ///< Detect and fill thin walls

    // ==============================================================================
    // General Processing Flags
    // ==============================================================================
    bool   adaptive_slicing = false;         ///< Enable adaptive layer thickness
    bool   complete_objects = true;          ///< Complete each object before next
    double duplicate_distance = 6.0;         ///< Distance between duplicated objects [mm]
    bool   match_horizontal_surfaces = true; ///< Align top/bottom surfaces to layer heights
    bool   interface_shells = true;          ///< Generate interface shells
};

} // namespace MarcSLM
