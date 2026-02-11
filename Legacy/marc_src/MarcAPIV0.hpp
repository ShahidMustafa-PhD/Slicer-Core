#ifndef SLIC3R_HPP
#define SLIC3R_HPP

#define BUILDING_MARC_DLL

#include "../libslic3r/ConfigBase.hpp"
#include "../libslic3r/IO.hpp"
#include "../libslic3r/Model.hpp"
#include "../libslic3r/Point.hpp"
#include "../libslic3r/Geometry.hpp"
#include "../libslic3r/Log.hpp"
#include "../libslic3r/SLAPrint.hpp"
#include "../libslic3r/Print.hpp"
#include "../libslic3r/SimplePrint.hpp"
#include "../libslic3r/TriangleMesh.hpp"
#include "../libslic3r/libslic3r.h"
#include "../marc_src/SlmPrint.hpp"
#include "../marc_dll/MarcAPIInterface.h"

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
    MarcErrorCode setModels(const PathList& _mesh_paths);
    MarcErrorCode setConfigs(const PathList& _configfiles_paths);
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

    // Progress callback
    void registerProgressCallback(ProgressCallback cb);

private:
    // Internal data
    Slic3r::ConfigDef config_def;
    Slic3r::ConfigDef cli_actions_config_def{};
    Slic3r::ConfigDef cli_transform_config_def{};
    Slic3r::ConfigDef cli_misc_config_def{};
    Slic3r::ConfigDef print_config_def{};

    Slic3r::DynamicConfig config;
    Slic3r::DynamicPrintConfig print_config;
    Slic3r::FullPrintConfig full_print_config;

   

    PathList model_paths;
    PathList configfiles_paths;
    StringList actions;
    StringList transforms;

    float bed_width;
    float bed_depth;
    float spacing;

    Slic3r::Model slm_model;
    std::string last_outfile;

    SlmPrint* slm_print = new SlmPrint();
    ProgressCallback progress_callback = nullptr;

    // Internal helper
    MarcErrorCode print_help(bool include_print_options = false);
    void report_progress(const char* message, int percent);
    Slic3r::DynamicPrintConfig  volumeconfig(std::string file);
    std::string output_filepath(const Model &model, IO::ExportFormat format) const;
};

} // namespace Marc

#endif // SLIC3R_HPP
