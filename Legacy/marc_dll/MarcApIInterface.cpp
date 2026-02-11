
#include "MarcAPIInterfaceInternal.h"
#include "MarcAPIInterface.h"
#include "../marc_src/MarcAPI.hpp"     // for MarcAPI, InternalModel
#include "../marc_src/ErrorState.hpp"  // for ErrorState
#include "../marc_src/Logger.hpp"      // for Logger
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <mutex>

#include <vector>
#include <string>
#include "MarcAPI.hpp"  // Make sure this defines Marc::InternalModel or InternalModel

// Assumes InternalModel is either in the global or Marc namespace
// Adjust InternalModel to Marc::InternalModel if it's namespaced

using namespace Marc;

namespace { 

    
// Internal representation of a model (example)



std::vector<InternalModel> convert_models_to_internal(const GuiDataArray& modelArray) {
    std::vector<InternalModel> result;
    result.reserve(modelArray.count);

    for (size_t i = 0; i < modelArray.count; ++i) {
        const GuiData& m = modelArray.models[i];

        InternalModel im;
        im.path = std::string(m.path);
        im.buildconfig = std::string(m.buildconfig);
       // im.stylesconfig = std::string(m.stylesconfig);
        im.number = m.number;
        im.xpos = m.xpos;
        im.ypos = m.ypos;
        im.zpos = m.zpos;
        im.roll = m.roll;
        im.pitch = m.pitch;
        im.yaw = m.yaw;

        result.push_back(im);
    }

    return result;
}



        // Convert MarcPathArray to std::vector<std::filesystem::path>
        inline std::vector<std::filesystem::path> convert_paths(const MarcPathArray& arr) {
            std::vector<std::filesystem::path> result;
            for (std::size_t i = 0; i < arr.count; ++i) {
                result.emplace_back(arr.paths[i]);
            }
            return result;
        }
    
        // Convert string array to std::vector<std::string>
        inline std::vector<std::string> convert_strings(const char** arr, std::size_t count) {
            std::vector<std::string> result;
            for (std::size_t i = 0; i < count; ++i) {
                result.emplace_back(arr[i]);
            }
            return result;
        }
    }
    
    extern "C" {
    static std::mutex g_api_gate; // serialize cross-DLL calls if needed
    const char* get_last_error_message() {
        return ErrorState::instance().get_last_error();
    }   
    
    // Create an instance of the MarcAPI class
    MARC_API MarcHandle create_marc_api(float bed_width, float bed_depth, float spacing) {
        try { std::lock_guard<std::mutex> lock(g_api_gate);
            return reinterpret_cast<MarcHandle>(new MarcAPI(bed_width, bed_depth, spacing));
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error creating MarcAPI instance: " + std::string(e.what()));
            Logger::instance().log("Error in create_marc_api: " + std::string(e.what()));
            return nullptr;
        }
    }
    
    // Destroy an instance of the MarcAPI class
    MARC_API void destroy_marc_api(MarcHandle handle) {
        try { std::lock_guard<std::mutex> lock(g_api_gate);
            if (handle) {
                delete reinterpret_cast<MarcAPI*>(handle);
            } else {
               ErrorState::instance().set_error("Attempt to destroy null MarcHandle.");
            Logger::instance().log("Attempt to destroy null MarcHandle.");
            }
        } catch (const std::exception& e) {
           ErrorState::instance().set_error("Error destroying MarcAPI instance: " + std::string(e.what()));
            Logger::instance().log("Error in destroy_marc_api: " + std::string(e.what()));
        }
    }
    
    // Set models for MarcAPI
    MARC_API MarcErrorCode set_models(MarcHandle handle, GuiDataArray models) {
        try { 
            if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to set_models.");
            return static_cast<MarcAPI*>(handle)->setModels(convert_models_to_internal(models));
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in set_models: " + std::string(e.what()));
            Logger::instance().log("Error in set_models: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Set configurations for MarcAPI
    MARC_API MarcErrorCode set_configs(MarcHandle handle, MarcPathArray configs) {
        try {
            if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to set_configs.");
            return static_cast<MarcAPI*>(handle)->setConfigs(convert_paths(configs));
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in set_configs: " + std::string(e.what()));
            Logger::instance().log("Error in set_configs: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    // MARC_API MarcErrorCode ;
    // Set json configurations for MarcAPI
      MARC_API MarcErrorCode set_config_json(MarcHandle handle,const char* filePath) {
        try {
            if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to set_configs.");
            return static_cast<MarcAPI*>(handle)->set_config_json(std::filesystem::path(filePath));
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in set_config_json: " + std::string(e.what()));
            Logger::instance().log("Error in set_config_json: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }

    // Set actions for MarcAPI
    MARC_API MarcErrorCode set_actions(MarcHandle handle, const char** actions, std::size_t count) {
        try {
            if (!handle)   return MARC_E_FAIL;// throw std::invalid_argument("Invalid MarcHandle passed to set_actions.");
            return static_cast<MarcAPI*>(handle)->setActions(convert_strings(actions, count));
        } catch (const std::exception& e) {
           ErrorState::instance().set_error("Error in set_actions: " + std::string(e.what()));
            Logger::instance().log("Error in set_actions: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Set transformations for MarcAPI
    MARC_API MarcErrorCode set_transforms(MarcHandle handle, const char** transforms, std::size_t count) {
        try {
            if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to set_transforms.");
            return static_cast<MarcAPI*>(handle)->setTransforms(convert_strings(transforms, count));
        } catch (const std::exception& e) {
           ErrorState::instance().set_error("Error in set_transforms: " + std::string(e.what()));
        Logger::instance().log("Error in set_transforms: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Perform model update in MarcAPI
    MARC_API MarcErrorCode update_model(MarcHandle handle) {
        try {
            if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to update_model.");
            std::lock_guard<std::mutex> lock(g_api_gate);
            return reinterpret_cast<MarcAPI*>(handle)->updateModel();
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in update_model: " + std::string(e.what()));
            Logger::instance().log("Error in update_model: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Perform model duplication
    MARC_API MarcErrorCode duplicate(MarcHandle handle) {
        try {
            if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to duplicate.");
            return static_cast<MarcAPI*>(handle)->duplicate();
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in duplicate: " + std::string(e.what()));
            Logger::instance().log("Error in duplicate: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Perform grid duplication
    MARC_API MarcErrorCode duplicate_grid(MarcHandle handle) {
        try {
            if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to duplicate_grid.");
            return static_cast<MarcAPI*>(handle)->duplicate_grid();
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in duplicate_grid: " + std::string(e.what()));
            Logger::instance().log("Error in duplicate_grid: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Operations for moving the model around the center
    MARC_API MarcErrorCode around_center(MarcHandle handle) {
        try {
            if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to around_center.");
            return static_cast<MarcAPI*>(handle)->around_center();
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in around_center: " + std::string(e.what()));
            Logger::instance().log("Error in around_center: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Align the model to X and Y axes
    MARC_API MarcErrorCode align_xy(MarcHandle handle, float px, float py) {
        try {
            if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to align_xy.");
            return static_cast<MarcAPI*>(handle)->alignXY(px, py);
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in alignxy: " + std::string(e.what()));
            Logger::instance().log("Error in alignxy: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Rotate the model around the X axis
    MARC_API MarcErrorCode rotate_x(MarcHandle handle, float angle) {
        try {
            if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to rotate_x.");
            return static_cast<MarcAPI*>(handle)->rotateX(angle);
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in rotate_x: " + std::string(e.what()));
            Logger::instance().log("Error in rotate_x: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Rotate the model around the Y axis
    MARC_API MarcErrorCode rotate_y(MarcHandle handle, float angle) {
        try {
            if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to rotate_y.");
            return static_cast<MarcAPI*>(handle)->rotateY(angle);
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in rotate_y: " + std::string(e.what()));
            Logger::instance().log("Error in rotate_y: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
    // Rotate the model around the Z axis
    MARC_API MarcErrorCode rotate_z(MarcHandle handle, float angle) {
        try {
            if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to rotate_z.");
            return static_cast<MarcAPI*>(handle)->rotateZ(angle);
        } catch (const std::exception& e) {
            ErrorState::instance().set_error("Error in rotate_z: " + std::string(e.what()));
            Logger::instance().log("Error in rotate_z: " + std::string(e.what()));
            return MARC_E_FAIL;
        }
    }
    
  
// Scale the model
MARC_API MarcErrorCode scale(MarcHandle handle, float s) {
    try {
        if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to scale.");
        return static_cast<MarcAPI*>(handle)->scale(s);
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in scale: " + std::string(e.what()));
        Logger::instance().log("Error in scale: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Scale to fit the model
MARC_API MarcErrorCode scale_to_fit(MarcHandle handle) {
    try {
        if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to scale_to_fit.");
        return static_cast<MarcAPI*>(handle)->scaleToFit();
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in scale_to_fit: " + std::string(e.what()));
        Logger::instance().log("Error in scale_to_fit: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Split the model
MARC_API MarcErrorCode split(MarcHandle handle) {
    try {
        if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to split.");
        return static_cast<MarcAPI*>(handle)->split();
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in split: " + std::string(e.what()));
        Logger::instance().log("Error in split: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Repair the model
MARC_API MarcErrorCode repair(MarcHandle handle) {
    try {
        if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to repair.");
        return static_cast<MarcAPI*>(handle)->repair();
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in repair: " + std::string(e.what()));
        Logger::instance().log("Error in repair: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Provide information about the model
MARC_API MarcErrorCode info(MarcHandle handle) {
    try {
        if (!handle) return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to info.");
        return static_cast<MarcAPI*>(handle)->info();
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in info: " + std::string(e.what()));
        Logger::instance().log("Error in info: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Print the model
MARC_API MarcErrorCode print(MarcHandle handle) {
    try {
        if (!handle) return MARC_E_FAIL;// throw std::invalid_argument("Invalid MarcHandle passed to print.");
        return static_cast<MarcAPI*>(handle)->print();
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in print: " + std::string(e.what()));
        Logger::instance().log("Error in print: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Export model to SLM file
MARC_API MarcErrorCode export_slm_file(MarcHandle handle) {
    try {
        if (!handle)   return MARC_E_FAIL;//throw std::invalid_argument("Invalid MarcHandle passed to export_slm_file.");
        std::lock_guard<std::mutex> lock(g_api_gate);
        return reinterpret_cast<MarcAPI*>(handle)->exportSlmFile();
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in export_slm_file: " + std::string(e.what()));
        Logger::instance().log("Error in export_slm_file: " + std::string(e.what()));
        return MARC_E_FAIL;
    }
}

// Register progress callback
MARC_API void register_progress_callback(MarcHandle handle, MarcProgressCallback callback) {
    try {
        if (!handle);// throw std::invalid_argument("Invalid MarcHandle passed to register_progress_callback.");
        static_cast<MarcAPI*>(handle)->registerProgressCallback(callback);
    } catch (const std::exception& e) {
        ErrorState::instance().set_error("Error in register_progress_callback: " + std::string(e.what()));
        Logger::instance().log("Error in register_progress_callback: " + std::string(e.what()));
    }
}


GuiDataArray get_models(MarcHandle handle) {
    GuiDataArray result{};
    // 1) Get your std::vector<InternalModel> or equivalent internal data here.
    std::vector<InternalModel> internal_models = static_cast<MarcAPI*>(handle)->get_internal_models(); //...

    // 2) Allocate array of mModel with malloc or new[]:
    result.count = internal_models.size();
    if (result.count > 0) {
        result.models = (GuiData*)malloc(sizeof(GuiData) * result.count);
        for (size_t i = 0; i < result.count; i++) {
            // copy fields from internal_models[i] to result.models[i]
            std::strncpy(result.models[i].path, internal_models[i].path.c_str(), sizeof(result.models[i].path) - 1);
            result.models[i].path[sizeof(result.models[i].path) - 1] = '\0';

            std::strncpy(result.models[i].buildconfig, internal_models[i].buildconfig.c_str(), sizeof(result.models[i].buildconfig) - 1);
            result.models[i].buildconfig[sizeof(result.models[i].buildconfig) - 1] = '\0';

            result.models[i].number = internal_models[i].number;
            result.models[i].xpos = internal_models[i].xpos;
            result.models[i].ypos = internal_models[i].ypos;
            result.models[i].zpos = internal_models[i].zpos;
            result.models[i].roll = internal_models[i].roll;
            result.models[i].pitch = internal_models[i].pitch;
            result.models[i].yaw = internal_models[i].yaw;
        }
    } else {
        result.models = nullptr;
    }
    return result;
}

MarcErrorCode free_model_array(GuiDataArray array) {
     try {
        if (array.models) {
            free(array.models);
        }
        return MARC_S_OK;
    } catch (const std::exception& e) {
        ErrorState::instance().set_error(std::string("Error in free_model_array: ") + e.what());
        Logger::instance().log(std::string("Error in free_model_array: ") + e.what());
        return MARC_E_FAIL;
    }


} // extern "C"
    }
