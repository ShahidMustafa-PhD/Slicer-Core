// Your derived class
#pragma once
#include "../libslic3r/SimplePrint.hpp"
#include "../libslic3r/Extruder.hpp"
#include "../libslic3r/Point.hpp"
//#include "../libslic3r/PrintConfig.hpp"
//#include "../libslic3r/PrintConfig.hpp"
#include "../libslic3r/Log.hpp"
#include "../libslic3r/Layer.hpp"
#include "../libslic3r/ExtrusionEntity.hpp"
#include "../libslic3r/ExtrusionEntityCollection.hpp"
#include "../libslic3r/SurfaceCollection.hpp"
#include <ctime>
#include <iostream>
#include "SVG.hpp"// Slm SVG class Modified.....
#include <iostream>
#include <string>
#include "BuildStyleConfigManager.h"
#include "MarcFile.hpp"
#include "ExportMarcSlice.hpp"
#include "slmHatches.hpp"

namespace Marc {

    class SlmPrint : public Slic3r::SimplePrint {
    public:
    SlmPrint() ;
    /// @brief 
    ~SlmPrint(){}

    void export_slm_svg();
    void export_slm_slice();
    void write_svg_layers(const std::string& outputDir);
    void readBuildStyles(const std::filesystem::path&  stylesfile_json);
    void readSlmConfig(const std::filesystem::path& configfile_json);
    void apply_all_configs();// applies the SlmConfig to all PrintObjects and PrintRegions
    void to_marc(const std::string& outputDir) ;
    void from_marc(const std::string& inputDir);
    void write_svg_slm_segments(const std::string& outputDir);

   private:
    Slic3r::PrintObject* object;
    std::vector<BuildStyle> buildStyles;
    SlmConfig jconfig;
    std::string role_color(const Slic3r::ExtrusionEntity* entity) const;
   
    
    
    //void process_entity(const ExtrusionEntity* entity, Marc::SVG *sv);
    //void process_entity(const ExtrusionEntity* entity, std::unique_ptr<Marc::SVG>& sv);
    
private:
    void processLayerRegions(const Slic3r::LayerRegionPtrs& regions, std::unique_ptr<Marc::SVG>& sv);
    void processHatches(const std::vector<SlmHatch>& hatches, std::unique_ptr<Marc::SVG>& sv);
    void processEntities(const Slic3r::ExtrusionEntitiesPtr& entities, std::unique_ptr<Marc::SVG>& sv);
    void processBridgedAreas(const Slic3r::Polylines& bridged, std::unique_ptr<Marc::SVG>& sv);
    void processUnsupportedBridgeEdges(const Slic3r::Polylines& edges, std::unique_ptr<Marc::SVG>& sv);
    void process_entity(const ExtrusionEntity* entity, std::unique_ptr<Marc::SVG>& sv);
    void process_surface(const Slic3r::Surface& surface, std::unique_ptr<Marc::SVG>& sv);
    void process_surface_collection(const Slic3r::SurfaceCollection& surfaces, std::unique_ptr<Marc::SVG>& sv);
    std::string surface_color(Slic3r::Surface* surface) const;

    void processSurfaceCollection(const SurfaceCollection& support_surfaces, std::unique_ptr<Marc::SVG>& sv);
    // In SlmPrint.hpp
void write_svg_support_layers(const std::string& outputDir);
void processSupportIslands(const ExPolygonCollection& islands, std::unique_ptr<Marc::SVG>& sv);
    std::string get_color_for_type(Marc::ThermalSegmentType type) const;

    };

}