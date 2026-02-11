#pragma once


#define BUILDING_MARC_DLL

//#include "../libslic3r/ConfigBase.hpp"
#include "../libslic3r/IO.hpp"
#include "../libslic3r/Model.hpp"
#include "../libslic3r/Point.hpp"
#include "../libslic3r/Geometry.hpp"
#include "../libslic3r/Log.hpp"
//#include "../libslic3r/SLAPrint.hpp"
#include "../libslic3r/Print.hpp"
#include "../libslic3r/SimplePrint.hpp"
#include "../libslic3r/TriangleMesh.hpp"
#include "../libslic3r/SlmConfig.hpp"
#include "../libslic3r/libslic3r.h"
#include "../marc_src/SlmPrint.hpp"
#include "../marc_dll/MarcAPIInterfaceInternal.h"

#include <cmath>
#include <chrono>
#include <cstdio>
#include <string>
#include <cstring>
#include <iostream>
#include <memory>
#include <filesystem>
#include <vector>
#include <functional>
#include <optional>

#if defined(_WIN32)
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
    #include <limits.h>
#elif defined(__linux__)
    #include <unistd.h>
    #include <limits.h>
#endif

//namespace fs = std::filesystem;
typedef int MarcErrorCode;

#define MARC_S_OK        0x00000000
#define MARC_E_FAIL      0x80004005
#define MARC_E_INVALID   0x80070057
#define MARC_E_NOTIMPL   0x80004001
namespace Marc {
//typedef int MarcErrorCode;
class MarcAPI {
public:
    using PathList = std::vector<std::filesystem::path>;
    using StringList = std::vector<std::string>;
    using ProgressCallback = std::function<void(const char*, int)>;
   
 

   
    MarcAPI(float bed_width, float bed_depth, float spacing = 5.0f)
        : bed_width(bed_width), bed_depth(bed_depth), spacing(spacing) {}

    ~MarcAPI();
    MarcErrorCode conFigure();
    MarcErrorCode conFigure_old();
    // Setters (for model/config/action/transform)
    MarcErrorCode setModels(const std::vector<InternalModel>& models);
    MarcErrorCode setConfigs(const PathList& _configfiles_paths);
    MarcErrorCode set_config_json(const std::filesystem::path filePath);

    MarcErrorCode setActions(const StringList& actions);
    MarcErrorCode setTransforms(const StringList& transforms);

    // Operations
    MarcErrorCode updateModel();
    MarcErrorCode updateModel_old(); // Old version for backward compatibility- Shahid
    MarcErrorCode duplicate();
    MarcErrorCode duplicate_grid();
    MarcErrorCode duplicate_model_objects_within_bed(); 
    MarcErrorCode around_center();
    MarcErrorCode alignXY(const float& px,const float& py);
    MarcErrorCode rotateX(const float& angle);
    MarcErrorCode rotateY(const float& angle);
    MarcErrorCode rotateZ(const float& angle);
    MarcErrorCode center_model_objects_around_point( const Slic3r::Pointf& P);
    MarcErrorCode cut(std::string opt_key,float xx ,std::vector<Model>& new_models);
    MarcErrorCode cut_grid(std::string opt_key,std::vector<Model>& new_models); 
    MarcErrorCode scale(const float& s);
    MarcErrorCode scaleToFit();
    MarcErrorCode split();
    MarcErrorCode save_conf();
    MarcErrorCode repair();
    MarcErrorCode info();
    MarcErrorCode print();
    MarcErrorCode exportslasvg();
    MarcErrorCode exportSlmFile();
    MarcErrorCode export_model(IO::ExportFormat format) ;
    MarcErrorCode arrangeBuildPlate();
    std::vector<InternalModel> get_internal_models() ;//= //...;

    // Progress callback
    void registerProgressCallback(ProgressCallback cb);

private:
    // Internal data
    //Slic3r::ConfigDef config_def;
    //Slic3r::ConfigDef cli_actions_config_def{};
    //Slic3r::ConfigDef cli_transform_config_def{};
    //Slic3r::ConfigDef cli_misc_config_def{};
    //Slic3r::ConfigDef print_config_def{};

    //Slic3r::DynamicConfig config;
    //Slic3r::DynamicPrintConfig print_config;
    //Slic3r::FullPrintConfig full_print_config;
    //Slic3r::PrintObjectConfig print_object_config;
    //Slic3r::PrintRegionConfig print_region_config;

   

    PathList model_paths;
    //PathList configfiles_paths;
    std::filesystem::path configfile_json;
    //std::filesystem::path stylesfile_json;
    StringList actions;
    StringList transforms;
    
    // Create a vector of InternalModel
    std::vector<InternalModel> m_models;

    float bed_width;
    float bed_depth;
    float spacing;

    std::unique_ptr<Slic3r::Model> slm_model = std::make_unique<Slic3r::Model>();
    std::string last_outfile;
   

    std::unique_ptr<SlmPrint> slm_print = std::make_unique<SlmPrint>();
    ProgressCallback progress_callback = nullptr;

    // Internal helper
    MarcErrorCode print_help(bool include_print_options = false);
    void report_progress(const char* message, int percent);
    //Slic3r::DynamicPrintConfig  volumeconfig(std::string file);
    std::string output_filepath(const Model &model, IO::ExportFormat format) const;
    inline std::filesystem::path getExecutableDir();
    //std::unique_ptr<Slic3r::Model> model_ptr = std::make_unique<Slic3r::Model>();
    //std::map<int, Slic3r::ModelObject*> id_to_obj_map;

std::pair<std::unique_ptr<Model>, std::map<int, ModelVolume*>> buildPlate;
// Accessing the values
    //std::unique_ptr<Model>& model = buildPlate.first;
    //std::map<int, ModelObject*>& obj_map = buildPlate.second;
    
std::optional<std::pair<std::unique_ptr<Model>, std::map<int, ModelVolume*>>>
create_build_plate();//const std::vector<InternalModel>& models_input);


};

} // namespace Marc
