#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>
#include <fstream>
#include "../libslic3r/SlmConfig.hpp"
#include <iostream>

struct BuildStyle {
    int id = 0;
    std::string name;
    std::string description;
    int laserId = 0;
    int laserMode = 0;
    double laserPower = 0.0;
    double laserFocus = 0.0;
    double laserSpeed = 0.0;
    double hatchSpacing = 0.0;
    double layerThickness = 0.0;
    double pointDistance = 0.0;
    int pointDelay = 0;
    int pointExposureTime = 0;
    double jumpSpeed = 0.0;
    double jumpDelay = 0.0;
    double hatchSize = 0.0;  // Optional field with default value
};

// Custom from_json function to handle missing fields gracefully
inline void from_json(const nlohmann::json& j, BuildStyle& style) {
    // Helper lambda to safely get a value if it exists, otherwise use default
    auto safeGet = [&j](const std::string& key, auto& value, const auto& defaultValue) {
        try {
            if (j.contains(key) && !j[key].is_null()) {
                value = j[key].get<std::decay_t<decltype(value)>>();
            } else {
                value = defaultValue;
            }
        } catch (const std::exception& e) {
            std::cerr << "Warning: Error parsing key '" << key << "': " << e.what() 
                      << ". Using default value." << std::endl;
            value = defaultValue;
        }
    };

    safeGet("id", style.id, 0);
    safeGet("name", style.name, "");
    safeGet("description", style.description, "");
    safeGet("laserId", style.laserId, 0);
    safeGet("laserMode", style.laserMode, 0);
    safeGet("laserPower", style.laserPower, 0.0);
    safeGet("laserFocus", style.laserFocus, 0.0);
    safeGet("laserSpeed", style.laserSpeed, 0.0);
    safeGet("hatchSpacing", style.hatchSpacing, 0.0);
    safeGet("layerThickness", style.layerThickness, 0.0);
    safeGet("pointDistance", style.pointDistance, 0.0);
    safeGet("pointDelay", style.pointDelay, 0);
    safeGet("pointExposureTime", style.pointExposureTime, 0);
    safeGet("jumpSpeed", style.jumpSpeed, 0.0);
    safeGet("jumpDelay", style.jumpDelay, 0.0);
    safeGet("hatchSize", style.hatchSize, 0.0);  // Safely handle missing hatchSize
}

// Custom to_json function
inline void to_json(nlohmann::json& j, const BuildStyle& style) {
    j = nlohmann::json{
        {"id", style.id},
        {"name", style.name},
        {"description", style.description},
        {"laserId", style.laserId},
        {"laserMode", style.laserMode},
        {"laserPower", style.laserPower},
        {"laserFocus", style.laserFocus},
        {"laserSpeed", style.laserSpeed},
        {"hatchSpacing", style.hatchSpacing},
        {"layerThickness", style.layerThickness},
        {"pointDistance", style.pointDistance},
        {"pointDelay", style.pointDelay},
        {"pointExposureTime", style.pointExposureTime},
        {"jumpSpeed", style.jumpSpeed},
        {"jumpDelay", style.jumpDelay},
        {"hatchSize", style.hatchSize}
    };
}

class ConfigurationManager {
public:
    std::vector<BuildStyle> buildStyles;

    bool loadFromFile(const std::filesystem::path& configfile_json) {
        try {
            //std::cout << "Loading styles from: " << configfile_json.string() << std::endl;
            std::ifstream input(configfile_json);
            if (!input.is_open()) {
                std::cerr << "Failed to open config file: " << configfile_json << std::endl;
                return false;
            }

            // Parse JSON with error handling
            nlohmann::json jsonData;
            try {
                input >> jsonData;
            } catch (const std::exception& e) {
                std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                input.close();
                return false;
            }
            input.close();

            // Extract buildStyles array
            if (jsonData.contains("buildStyles") && jsonData["buildStyles"].is_array()) {
                try {
                    // Clear existing styles and reserve space for new ones
                    buildStyles.clear();
                    buildStyles.reserve(jsonData["buildStyles"].size());
                    
                    // Process each style
                    for (const auto& styleJson : jsonData["buildStyles"]) {
                        try {
                            BuildStyle style;
                            from_json(styleJson, style);
                            buildStyles.push_back(style);
                        } catch (const std::exception& e) {
                            std::cerr << "Warning: Error parsing build style: " << e.what() << std::endl;
                            // Continue processing other styles
                        }
                    }
                    
                    std::cout << "Successfully loaded " << buildStyles.size() << " build styles" << std::endl;
                } catch (const std::exception& e) {
                    std::cerr << "Error processing build styles: " << e.what() << std::endl;
                }
            } else {
                std::cerr << "JSON file does not contain 'buildStyles' array" << std::endl;
                return false;
            }

            return !buildStyles.empty();
        }
        catch (const std::exception& e) {
            std::cerr << "Unexpected error loading build styles: " << e.what() << std::endl;
            return false;
        }
    }

    bool saveToFile(const std::filesystem::path& configfile_json) const {
        try {
            nlohmann::json jsonData;
            jsonData["buildStyles"] = buildStyles;

            std::ofstream output(configfile_json);
            if (!output.is_open()) {
                std::cerr << "Failed to open output file: " << configfile_json << std::endl;
                return false;
            }

            output << jsonData.dump(4); // pretty print with 4 spaces
            output.close();
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "Error saving build styles: " << e.what() << std::endl;
            return false;
        }
    }
    
    std::vector<BuildStyle> getStyles() {
        return this->buildStyles;
    }
};

namespace Marc {
    class SlmConfigReader {
    public:
        // Load config from JSON file
        static bool loadFromFile(const std::string& filepath, SlmConfig& config) {
            try {
               // std::cout << "Loading SLM config from: " << filepath << std::endl;
                std::ifstream file(filepath);
                if (!file.is_open()) {
                    std::cerr << "Failed to open config file: " << filepath << std::endl;
                    return false;
                }

                nlohmann::json j;
                try {
                    file >> j;
                } catch (const std::exception& e) {
                    std::cerr << "Error parsing JSON: " << e.what() << std::endl;
                    file.close();
                    return false;
                }
                file.close();
                
                return loadFromJson(j, config);
            }
            catch (const std::exception& e) {
                std::cerr << "Error loading config file: " << e.what() << std::endl;
                return false;
            }
        }

        // Load config from JSON string
        static bool loadFromString(const std::string& json_str, SlmConfig& config) {
            try {
                nlohmann::json j = nlohmann::json::parse(json_str);
                return loadFromJson(j, config);
            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing JSON string: " << e.what() << std::endl;
                return false;
            }
        }

    private:
        // Helper function to load config from JSON object
        static bool loadFromJson(const nlohmann::json& j, SlmConfig& config) {
            // CONSTANT settings
            getValueIfExists(j, "beam_diameter", config.beam_diameter);
            getValueIfExists(j, "layer_thickness", config.layer_thickness);
            getValueIfExists(j, "first_layer_thickness", config.first_layer_thickness);
            getValueIfExists(j, "hatch_spacing", config.hatch_spacing);
            getValueIfExists(j, "hatch_angle", config.hatch_angle);
            getValueIfExists(j, "island_width", config.island_width);
            getValueIfExists(j, "island_height", config.island_height);
            getValueIfExists(j, "threads", config.threads);
            getValueIfExists(j, "z_steps_per_mm", config.z_steps_per_mm);
            getValueIfExists(j, "anchors", config.anchors);
            getValueIfExists(j, "perimeters", config.perimeters);
            getValueIfExists(j, "anchors_layer_thickness", config.anchors_layer_thickness);
            getValueIfExists(j, "xy_size_compensation", config.xy_size_compensation);
            getValueIfExists(j, "regions_overlap", config.regions_overlap);
            getValueIfExists(j, "overhangs_angle", config.overhangs_angle);
            getValueIfExists(j, "external_perimeters_first", config.external_perimeters_first);
            
            // SUPPORT SETTINGS
            getValueIfExists(j, "perimeter_hatch_spacing", config.perimeter_hatch_spacing);
            getValueIfExists(j, "support_material_spacing", config.support_material_spacing);
            getValueIfExists(j, "support_material_angle", config.support_material_angle);
            getValueIfExists(j, "support_material_model_clearance", config.support_material_model_clearance);
            getValueIfExists(j, "support_material_pillar_size", config.support_material_pillar_size);
            getValueIfExists(j, "support_material_pillar_spacing", config.support_material_pillar_spacing);
            
            // Additional settings from the full struct file
            getValueIfExists(j, "adaptive_slicing", config.adaptive_slicing);
            getValueIfExists(j, "fill_density", config.fill_density);
            getValueIfExists(j, "complete_objects", config.complete_objects);
            getValueIfExists(j, "duplicate_distance", config.duplicate_distance);
            getValueIfExists(j, "fill_gaps", config.fill_gaps);
            getValueIfExists(j, "extra_perimeters", config.extra_perimeters);
            getValueIfExists(j, "thin_walls", config.thin_walls);
            getValueIfExists(j, "infill_first", config.infill_first);
            getValueIfExists(j, "overhangs", config.overhangs);
            getValueIfExists(j, "support_material", config.support_material);
            getValueIfExists(j, "match_horizontal_surfaces", config.match_horizontal_surfaces);
            getValueIfExists(j, "interface_shells", config.interface_shells);
            getValueIfExists(j, "support_material_max_layers", config.support_material_max_layers);
            getValueIfExists(j, "support_material_enforce_layers", config.support_material_enforce_layers);
            getValueIfExists(j, "support_material_threshold", config.support_material_threshold);
            
            return true;
        }
        
        // Helper to safely extract value if it exists in JSON object
        template<typename T>
        static void getValueIfExists(const nlohmann::json& j, const std::string& key, T& value) {
            try {
                if (j.contains(key) && !j[key].is_null()) {
                    value = j[key].get<T>();
                }
                // If key doesn't exist or is null, keep the default value
            } catch (const std::exception& e) {
                std::cerr << "Warning: Error parsing key '" << key << "': " << e.what() 
                          << ". Using default value." << std::endl;
                // Keep the default value
            }
        }
    };

}  // namespace Marc