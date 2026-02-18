// ==============================================================================
// MarcSLM - SLM Config Reader Implementation
// ==============================================================================
// Minimal JSON parser — reads key-value pairs from a flat JSON object.
// For production robustness, consider linking nlohmann::json via vcpkg.
// ==============================================================================

#include "MarcSLM/Core/SlmConfigReader.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace MarcSLM {

namespace {

// Trim whitespace from both ends
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n\"");
    auto end   = s.find_last_not_of(" \t\r\n\"");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Try to extract a double value for a given key from a JSON-ish string
bool tryGetDouble(const std::string& json, const std::string& key, double& out) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;

    auto valStart = json.find_first_not_of(" \t\r\n", pos + 1);
    if (valStart == std::string::npos) return false;

    auto valEnd = json.find_first_of(",}\r\n", valStart);
    std::string valStr = trim(json.substr(valStart, valEnd - valStart));

    try {
        out = std::stod(valStr);
        return true;
    } catch (...) {
        return false;
    }
}

bool tryGetInt(const std::string& json, const std::string& key, int& out) {
    double d = 0.0;
    if (tryGetDouble(json, key, d)) {
        out = static_cast<int>(d);
        return true;
    }
    return false;
}

bool tryGetBool(const std::string& json, const std::string& key, bool& out) {
    std::string searchKey = "\"" + key + "\"";
    auto pos = json.find(searchKey);
    if (pos == std::string::npos) return false;

    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;

    auto valStart = json.find_first_not_of(" \t\r\n", pos + 1);
    if (valStart == std::string::npos) return false;

    auto valEnd = json.find_first_of(",}\r\n", valStart);
    std::string valStr = trim(json.substr(valStart, valEnd - valStart));

    // Lowercase for comparison
    std::transform(valStr.begin(), valStr.end(), valStr.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (valStr == "true" || valStr == "1") { out = true;  return true; }
    if (valStr == "false" || valStr == "0") { out = false; return true; }
    return false;
}

void applyJsonToConfig(const std::string& json, SlmConfig& config) {
    // Laser / Beam
    tryGetDouble(json, "beam_diameter",        config.beam_diameter);

    // Layer
    tryGetDouble(json, "layer_thickness",      config.layer_thickness);
    tryGetDouble(json, "first_layer_thickness", config.first_layer_thickness);

    // Hatch
    tryGetDouble(json, "hatch_spacing",        config.hatch_spacing);
    tryGetDouble(json, "hatch_angle",          config.hatch_angle);
    tryGetDouble(json, "island_width",         config.island_width);
    tryGetDouble(json, "island_height",        config.island_height);

    // Perimeters
    tryGetInt(json,    "perimeters",           config.perimeters);
    tryGetDouble(json, "perimeter_hatch_spacing", config.perimeter_hatch_spacing);
    tryGetBool(json,   "external_perimeters_first", config.external_perimeters_first);

    // Processing
    tryGetInt(json,    "threads",              config.threads);
    tryGetDouble(json, "z_steps_per_mm",       config.z_steps_per_mm);

    // Compensation
    tryGetDouble(json, "xy_size_compensation", config.xy_size_compensation);
    tryGetDouble(json, "regions_overlap",      config.regions_overlap);

    // Overhangs
    tryGetDouble(json, "overhangs_angle",      config.overhangs_angle);
    tryGetBool(json,   "overhangs",            config.overhangs);

    // Anchors
    tryGetDouble(json, "anchors",              config.anchors);
    tryGetDouble(json, "anchors_layer_thickness", config.anchors_layer_thickness);

    // Support
    tryGetBool(json,   "support_material",     config.support_material);
    tryGetDouble(json, "support_material_spacing", config.support_material_spacing);
    tryGetDouble(json, "support_material_angle",   config.support_material_angle);
    tryGetDouble(json, "support_material_model_clearance", config.support_material_model_clearance);
    tryGetDouble(json, "support_material_pillar_size",     config.support_material_pillar_size);
    tryGetDouble(json, "support_material_pillar_spacing",  config.support_material_pillar_spacing);
    tryGetDouble(json, "support_material_threshold",       config.support_material_threshold);
    tryGetInt(json,    "support_material_max_layers",      config.support_material_max_layers);
    tryGetInt(json,    "support_material_enforce_layers",   config.support_material_enforce_layers);

    // Fill
    tryGetInt(json,    "fill_density",         config.fill_density);
    tryGetBool(json,   "fill_gaps",            config.fill_gaps);
    tryGetBool(json,   "infill_first",         config.infill_first);
    tryGetBool(json,   "extra_perimeters",     config.extra_perimeters);
    tryGetBool(json,   "thin_walls",           config.thin_walls);

    // General
    tryGetBool(json,   "adaptive_slicing",     config.adaptive_slicing);
    tryGetBool(json,   "complete_objects",      config.complete_objects);
    tryGetDouble(json, "duplicate_distance",   config.duplicate_distance);
    tryGetBool(json,   "match_horizontal_surfaces", config.match_horizontal_surfaces);
    tryGetBool(json,   "interface_shells",     config.interface_shells);
}

} // anonymous namespace

bool SlmConfigReader::loadFromFile(const std::string& filepath, SlmConfig& config) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "SlmConfigReader: Failed to open " << filepath << std::endl;
            return false;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        std::string json = ss.str();
        file.close();

        applyJsonToConfig(json, config);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SlmConfigReader: " << e.what() << std::endl;
        return false;
    }
}

bool SlmConfigReader::loadFromString(const std::string& jsonStr, SlmConfig& config) {
    try {
        applyJsonToConfig(jsonStr, config);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "SlmConfigReader: " << e.what() << std::endl;
        return false;
    }
}

} // namespace MarcSLM
