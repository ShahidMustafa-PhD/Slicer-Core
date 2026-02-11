#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <memory>
#include <functional>
#include "../marc_dll/MarcAPIInterfaceInternal.h"
//#include "../marc_src/MarcAPI.hpp" // <-- Only CPP sees real MarcAPI
typedef int MarcErrorCode;

#define MARC_S_OK        0x00000000
#define MARC_E_FAIL      0x80004005
#define MARC_E_INVALID   0x80070057
#define MARC_E_NOTIMPL   0x80004001

namespace Marc {

class MarcAPIManager {
public:
    using PathList = std::vector<std::filesystem::path>;
    using StringList = std::vector<std::string>;
    using ProgressCallback = std::function<void(const char*, int)>;

    MarcAPIManager(float bed_width, float bed_depth, float spacing = 5.0f);
    ~MarcAPIManager();

    // Forwarded API
    MarcErrorCode conFigure();
    MarcErrorCode conFigure_old();
    //MarcErrorCode setModels(const PathList& mesh_paths);
    MarcErrorCode setModels(const std::vector<InternalModel>& models);
    MarcErrorCode setConfigs(const PathList& configfiles_paths);
    MarcErrorCode setActions(const StringList& actions);
    MarcErrorCode setTransforms(const StringList& transforms);
    MarcErrorCode updateModel();
    MarcErrorCode updateModel_old();
    MarcErrorCode duplicate();
    MarcErrorCode duplicate_grid();
    MarcErrorCode duplicate_model_objects_within_bed();
    MarcErrorCode around_center();
    MarcErrorCode alignXY(const float& px, const float& py);
    MarcErrorCode rotateX(const float& angle);
    MarcErrorCode rotateY(const float& angle);
    MarcErrorCode rotateZ(const float& angle);
    //MarcErrorCode center_model_objects_around_point(const Slic3r::Pointf& P);
    //MarcErrorCode cut(std::string opt_key, float xx, std::vector<Model>& new_models);
    //MarcErrorCode cut_grid(std::string opt_key, std::vector<Model>& new_models);
    MarcErrorCode scale(const float& s);
    MarcErrorCode scaleToFit();
    MarcErrorCode split();
    MarcErrorCode save_conf();
    MarcErrorCode repair();
    MarcErrorCode info();
    MarcErrorCode print();
    MarcErrorCode exportslasvg();
    MarcErrorCode exportSlmFile();
    //MarcErrorCode export_model(IO::ExportFormat format);

    void registerProgressCallback(ProgressCallback cb);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace Marc
