// ==============================================================================
// MarcSLM - Internal Model Descriptor
// ==============================================================================
// Ported from Legacy MarcAPIInterfaceInternal.h
// Represents a single model with its placement and orientation on the build plate
// ==============================================================================

#pragma once

#include <string>

namespace MarcSLM {

/// @brief Descriptor for a single 3D model to be placed on the build plate.
/// @details Contains the file path, build configuration reference, placement
///          coordinates, and Euler angles for orientation.
///          Ported from Legacy Marc::InternalModel.
struct InternalModel {
    std::string path;              ///< Path to 3D model file (STL, 3MF, OBJ, etc.)
    std::string buildconfig;       ///< Path to JSON build configuration file
    int         number = 0;        ///< Model/part number (unique ID on build plate)
    double      xpos = 0.0;       ///< X position on build plate [mm]
    double      ypos = 0.0;       ///< Y position on build plate [mm]
    double      zpos = 0.0;       ///< Z position on build plate [mm]
    double      roll = 0.0;       ///< Roll rotation (X-axis) [radians]
    double      pitch = 0.0;      ///< Pitch rotation (Y-axis) [radians]
    double      yaw = 0.0;        ///< Yaw rotation (Z-axis) [radians]
};

} // namespace MarcSLM
