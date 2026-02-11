#include "MarcAPIManager.hpp"
#include "../marc_src/MarcAPI.hpp" // <-- Only CPP sees real MarcAPI

namespace Marc {

// Definition of private Impl class
class MarcAPIManager::Impl {
public:
    Impl(float bed_width, float bed_depth, float spacing)
        : api(std::make_unique<MarcAPI>(bed_width, bed_depth, spacing)) {}

    std::unique_ptr<MarcAPI> api;
};

// Constructor and Destructor
MarcAPIManager::MarcAPIManager(float bed_width, float bed_depth, float spacing)
    : impl(std::make_unique<Impl>(bed_width, bed_depth, spacing)) {}

MarcAPIManager::~MarcAPIManager() = default;

// Forward all functions
MarcErrorCode MarcAPIManager::conFigure() { return impl->api->conFigure(); }
MarcErrorCode MarcAPIManager::conFigure_old() { return impl->api->conFigure_old(); }
MarcErrorCode MarcAPIManager::setModels(const std::vector<InternalModel>& models) { return impl->api->setModels(models); }
MarcErrorCode MarcAPIManager::setConfigs(const PathList& paths) { return impl->api->setConfigs(paths); }
MarcErrorCode MarcAPIManager::setActions(const StringList& actions) { return impl->api->setActions(actions); }
MarcErrorCode MarcAPIManager::setTransforms(const StringList& transforms) { return impl->api->setTransforms(transforms); }
MarcErrorCode MarcAPIManager::updateModel() { return impl->api->updateModel(); }
//MarcErrorCode MarcAPIManager::updateModel_old() { return impl->api->updateModel_old(); }
MarcErrorCode MarcAPIManager::duplicate() { return impl->api->duplicate(); }
MarcErrorCode MarcAPIManager::duplicate_grid() { return impl->api->duplicate_grid(); }
MarcErrorCode MarcAPIManager::duplicate_model_objects_within_bed() { return impl->api->duplicate_model_objects_within_bed(); }
MarcErrorCode MarcAPIManager::around_center() { return impl->api->around_center(); }
MarcErrorCode MarcAPIManager::alignXY(const float& px, const float& py) { return impl->api->alignXY(px, py); }
MarcErrorCode MarcAPIManager::rotateX(const float& angle) { return impl->api->rotateX(angle); }
MarcErrorCode MarcAPIManager::rotateY(const float& angle) { return impl->api->rotateY(angle); }
MarcErrorCode MarcAPIManager::rotateZ(const float& angle) { return impl->api->rotateZ(angle); }
//MarcErrorCode MarcAPIManager::center_model_objects_around_point(const Slic3r::Pointf& P) { return impl->api->center_model_objects_around_point(P); }
//MarcErrorCode MarcAPIManager::cut(std::string opt_key, float xx, std::vector<Model>& new_models) { return impl->api->cut(opt_key, xx, new_models); }
//MarcErrorCode MarcAPIManager::cut_grid(std::string opt_key, std::vector<Model>& new_models) { return impl->api->cut_grid(opt_key, new_models); }
MarcErrorCode MarcAPIManager::scale(const float& s) { return impl->api->scale(s); }
MarcErrorCode MarcAPIManager::scaleToFit() { return impl->api->scaleToFit(); }
MarcErrorCode MarcAPIManager::split() { return impl->api->split(); }
MarcErrorCode MarcAPIManager::save_conf() { return impl->api->save_conf(); }
MarcErrorCode MarcAPIManager::repair() { return impl->api->repair(); }
MarcErrorCode MarcAPIManager::info() { return impl->api->info(); }
MarcErrorCode MarcAPIManager::print() { return impl->api->print(); }
MarcErrorCode MarcAPIManager::exportslasvg() { return impl->api->exportslasvg(); }
MarcErrorCode MarcAPIManager::exportSlmFile() { return impl->api->exportSlmFile(); }
//MarcErrorCode MarcAPIManager::export_model(IO::ExportFormat format) { return impl->api->export_model(format); }

void MarcAPIManager::registerProgressCallback(ProgressCallback cb) {
    impl->api->registerProgressCallback(cb);
}

} // namespace Marc
