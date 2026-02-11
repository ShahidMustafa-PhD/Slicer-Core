#include "SlmPrint.hpp"
#include <iostream>
#include "scanSegments.hpp"
    // Implementation of the methods
namespace Marc 
{
  

    SlmPrint::SlmPrint()
    {
    }

    void SlmPrint::export_slm_svg()
    {
        // Your SVG export logic here
    }

    void SlmPrint::export_slm_slice() 
    {
        // Your slice export logic here


    }

    // Function to iterate through the layers and generate SVG for each layer

void SlmPrint::write_svg_layers(const std::string& outputDir) 
{
    // Create output directory for SVG layers
    std::filesystem::path svgDir = std::filesystem::path(outputDir) / "SvgLayers";
    std::filesystem::create_directories(svgDir);
    std::cout << "Output Directory: " << outputDir << std::endl;

    // Process print data
    Slic3r::Print& print = this->print();
    print.status_cb = this->status_cb;
    //print.validate();
    print.process(); // Fill all data structures in the print object
    
    if (print.status_cb != nullptr) {
        print.status_cb(90, "Exporting SLM svg");
    }
    
    // Perform DMLS segmentation into thermal segments (non-invasive)
    {
        Slic3r::PrintObject* obj = print.get_object(0);
        if (obj) {
            Marc::scanSegments segmenter(obj, this->jconfig);
            segmenter.segment_all();
        }
    }
    
    // Get first print object
    object = print.get_object(0);
    if (!object) {
        std::cerr << "Error: No print object found" << std::endl;
        return;
    }

    // Iterate through each layer and generate SVG
    for (const auto& layer : object->layers) {
        // Create SVG file path
        std::string outputPath = (svgDir / ("Layer" + std::to_string(layer->id()) + ".svg")).string();
        
        // Create SVG with smart pointer
        auto sv = std::make_unique<Marc::SVG>(outputPath.c_str());
        
        // Draw basic elements
        sv->draw(Slic3r::Point(0, 0), "red", scale_(0.1));
        sv->draw(Slic3r::Point(0, 0), "none", scale_(80));
        
        // Process layer regions
        processLayerRegions(layer->regions, sv);
    }
    //write_svg_support_layers(outputDir);

    this->to_marc(outputDir);// Convert the print object to MARC format
    this->from_marc(outputDir);// Read the MARC file back to ensure correctness
   
    write_svg_slm_segments(outputDir);

    // Clean up
    object->clear_layers();
    object->clear_support_layers();
}

void SlmPrint::processLayerRegions(const Slic3r::LayerRegionPtrs& regions, 
                                  std::unique_ptr<Marc::SVG>& sv) 
{
    for (const auto& layerm : regions) {
        // Process hatches
        processHatches(layerm->hatches, sv);
        
        // Process perimeters
        if (!layerm->perimeters.entities.empty()) {
            processEntities(layerm->perimeters.entities, sv);
        }
        
        // Process thin fills
        if (!layerm->thin_fills.entities.empty()) {
            processEntities(layerm->thin_fills.entities, sv);
        }
        
        // void processSurfaceCollection(const SurfaceCollection& support_surfaces, std::unique_ptr<Marc::SVG>& sv);
        // Process support surfaces - add this line
        processSurfaceCollection(layerm->support_surfaces, sv);
        
         //SurfaceCollection support_surfaces;
        // Process bridged areas
       // processBridgedAreas(layerm->bridged, sv);
        
        // Process unsupported bridge edges
        //processUnsupportedBridgeEdges(layerm->unsupported_bridge_edges.polylines, sv);
    }
}

void SlmPrint::processHatches(const std::vector<SlmHatch>& hatches, 
                             std::unique_ptr<Marc::SVG>& sv)
{
    for (const auto& hatch : hatches) {
        for (const auto& line : hatch.lines()) {
            auto translated_line = line;
            translated_line.translate(object->_copies_shift.x, object->_copies_shift.y);
            sv->draw(translated_line, "green", 0.4);
        }
    }
}

void SlmPrint::processSurfaceCollection(const SurfaceCollection& support_surfaces, 
                                     std::unique_ptr<Marc::SVG>& sv)
{
    if (support_surfaces.surfaces.empty()) {
        return;  // No support surfaces to process
    }
    
    for (const auto& surface : support_surfaces.surfaces) {
        // Get the expolygon from the surface
        auto expolygon = surface.expolygon;
        
        // Process contour (outer boundary)
        auto translated_contour = expolygon.contour;
        translated_contour.translate(object->_copies_shift);
        
        // Choose color based on surface type
        std::string color = "purple";  // Default color for supports
        
        // Use different colors for different support surface types
        if (surface.is_solid()) {
            color = "turquoise";  // Solid support interface
        } else if (surface.is_bridge()) {
            color = "deeppink";  // Bridge support
        }
        
        // Draw contour polygon
        sv->draw(translated_contour, color, 0.5);
        
        // Process holes
        for (const auto& hole : expolygon.holes) {
            auto translated_hole = hole;
            translated_hole.translate(object->_copies_shift);
            sv->draw(translated_hole, color, 0.5);
        }
    }
}
void SlmPrint::processEntities(const Slic3r::ExtrusionEntitiesPtr& entities,
                              std::unique_ptr<Marc::SVG>& sv) 
{
    for (const ExtrusionEntity* entity : entities) {
        process_entity(entity, sv);
    }
}

void SlmPrint::processBridgedAreas(const Slic3r::Polylines& bridged, 
                                  std::unique_ptr<Marc::SVG>& sv)
{
    for (const auto& polygon : bridged) {
        auto translated_path = polygon;
        translated_path.translate(object->_copies_shift);
        sv->draw(translated_path, "black", 1.0);
    }
}

void SlmPrint::processUnsupportedBridgeEdges(const Slic3r::Polylines& edges,
                                           std::unique_ptr<Marc::SVG>& sv)
{
    for (const auto& polyline : edges) {
        auto translated_path = polyline;
        translated_path.translate(object->_copies_shift);
        sv->draw(translated_path, "blue", 1.0);
    }
}

void SlmPrint::process_entity(const ExtrusionEntity* entity, std::unique_ptr<Marc::SVG>& sv) 
{
    if (!entity || !sv) {
        return;  // Handle null pointers safely
    }
    
    if (const ExtrusionPath* path = dynamic_cast<const ExtrusionPath*>(entity)) {
        auto translated_path = path->polyline;
        translated_path.translate(object->_copies_shift);
        sv->draw(translated_path, role_color(entity), 0.2);
    }
    else if (const ExtrusionLoop* loop = dynamic_cast<const ExtrusionLoop*>(entity)) {
        for (const ExtrusionPath& path : loop->paths) {
            auto translated_path = path.polyline;
            translated_path.translate(object->_copies_shift);
            sv->draw(translated_path, role_color(&path), 0.2);
        }
    }
    else if (const ExtrusionEntityCollection* collection = dynamic_cast<const ExtrusionEntityCollection*>(entity)) {
        for (const ExtrusionEntity* sub_entity : collection->entities) {
            process_entity(sub_entity, sv);  // Recursive call
        }
    }
}

std::string SlmPrint::role_color(const Slic3r::ExtrusionEntity *entity) const
{
    if (!entity) {
        return "none";
    }
    
    // Use a map for cleaner role-to-color mapping
    static const std::unordered_map<ExtrusionRole, std::string> role_colors = {
        {erPerimeter, "black"},
        {erExternalPerimeter, "darkmagenta"},
        {erOverhangPerimeter, "red"},
        {erInternalInfill, "blue"},
        {erSolidInfill, "green"},
        {erTopSolidInfill, "tomato"},
        {erBridgeInfill, "deeppink"},
        {erGapFill, "grey"},
        {erSkirt, "indigo"},
        {erSupportMaterial, "turquoise"},
        {erSupportMaterialInterface, "springgreen"},
        {erNone, "yellow"}
    };
    
    auto it = role_colors.find(entity->get_role());
    return it != role_colors.end() ? it->second : "olive";
}

std::string SlmPrint::surface_color(Slic3r::Surface* surface) const
{
    if (!surface) {
        return "red";
    }
    
    // Use a map for cleaner surface type-to-color mapping
    static const std::unordered_map<SurfaceType, std::string> surface_colors = {
        {stTop, "bisque"},
        {stBottom, "darkorange"},
        {stInternal, "blueviolet"},
        {stSolid, "chartreuse"},
        {stBridge, "darkcyan"},
        {stVoid, "black"}
    };
    
    auto it = surface_colors.find(surface->surface_type);
    return it != surface_colors.end() ? it->second : "darkmagenta";
}

void SlmPrint::write_svg_support_layers(const std::string& outputDir)
{
    // Create output directory for support SVG files
    std::filesystem::path supportDir = std::filesystem::path(outputDir) / "Supports";
    std::filesystem::create_directories(supportDir);
    std::cout << "Support Output Directory: " << supportDir.string() << std::endl;
    
    // Get print object
    Slic3r::Print& print = this->print();
    object = print.get_object(0);
    if (!object) {
        std::cerr << "Error: No print object found" << std::endl;
        return;
    }
    
    // Check if support layers exist
    if (object->support_layers.empty()) {
        std::cout << "No support layers found" << std::endl;
        return;
    }

    // Iterate through each support layer and generate SVG
    for (const auto& support_layer : object->support_layers) {
        // Create SVG file path
        std::string outputPath = (supportDir / ("Support_" + std::to_string(support_layer->id()) + ".svg")).string();
        
        // Create SVG with smart pointer
        auto sv = std::make_unique<Marc::SVG>(outputPath.c_str());
        
        // Draw basic elements
        sv->draw(Slic3r::Point(0, 0), "red", scale_(0.1));
        sv->draw(Slic3r::Point(0, 0), "none", scale_(80));
        
        // Process support islands
        processSupportIslands(support_layer->support_islands, sv);
        
        // Process support fills
        if (!support_layer->support_fills.entities.empty()) {
            processEntities(support_layer->support_fills.entities, sv);
        }
        
        // Process support interface fills
        if (!support_layer->support_interface_fills.entities.empty()) {
            processEntities(support_layer->support_interface_fills.entities, sv);
        }
    }
    
    // Note: We don't clear support layers here as this might be called
    // before or after write_svg_layers, which already handles cleanup
}

void SlmPrint::processSupportIslands(const ExPolygonCollection& islands, 
                                   std::unique_ptr<Marc::SVG>& sv)
{
    for (const auto& expoly : islands) {
        // Process contour
        auto translated_contour = expoly.contour;
        translated_contour.translate(object->_copies_shift);
        sv->draw(translated_contour, "orange", 0.5);
        
        // Process holes
        for (const auto& hole : expoly.holes) {
            auto translated_hole = hole;
            translated_hole.translate(object->_copies_shift);
            sv->draw(translated_hole, "orange", 0.5);
        }
    }
}

void SlmPrint::write_svg_slm_segments(const std::string& outputDir)
{
    // Create output directory for SLM segment SVG files
    std::filesystem::path svgDir = std::filesystem::path(outputDir) / "SvgLayers_slm";
    std::filesystem::create_directories(svgDir);
    std::cout << "SLM Segments Output Directory: " << svgDir.string() << std::endl;

    // Get print object
    if (!object) {
        std::cerr << "Error: No print object found for SLM segments SVG export" << std::endl;
        return;
    }

    // Iterate through each layer and generate SVG for thermal segments
    for (const auto& layer : object->layers) {
        // Create SVG file path
        std::string outputPath = (svgDir / ("Layer" + std::to_string(layer->id()) + ".svg")).string();
        
        // Create SVG with smart pointer
        auto sv = std::make_unique<Marc::SVG>(outputPath.c_str());
        
        // Draw basic elements
        sv->draw(Slic3r::Point(0, 0), "red", scale_(0.1));
        sv->draw(Slic3r::Point(0, 0), "none", scale_(80));
        
        // Process layer regions for thermal segments
        for (const auto& region : layer->regions) {
            // Draw segment hatches
            for (const auto& seg_hatch : region->segment_hatches_) {
                std::string color = get_color_for_type(seg_hatch.type);
                for (const auto& line : seg_hatch.hatches) {
                    auto translated_line = line;
                    translated_line.translate(object->_copies_shift.x, object->_copies_shift.y);
                    sv->draw(translated_line, color, 0.4);
                }
            }
            
            // Draw segment polylines
            for (const auto& seg_poly : region->segment_polylines_) {
                std::string color = get_color_for_type(seg_poly.type);
                for (const auto& poly : seg_poly.polys) {
                    auto translated_poly = poly;
                    translated_poly.translate(object->_copies_shift);
                    sv->draw(translated_poly, color, 0.2);
                }
            }
        }
    }
}

std::string SlmPrint::get_color_for_type(Marc::ThermalSegmentType type) const
{
    // Define distinct colors for each ThermalSegmentType
    static const std::unordered_map<Marc::ThermalSegmentType, std::string> segment_colors = {
        {Marc::CoreContour_Volume, "black"},
        {Marc::CoreContour_Overhang, "maroon"},
        {Marc::HollowShell1Contour_Volume, "blue"},
        {Marc::HollowShell1Contour_Overhang, "darkblue"},
        {Marc::HollowShell2Contour_Volume, "yellow"},
        {Marc::HollowShell2Contour_Overhang, "darkgreen"},
        {Marc::CoreOverhangHatch, "orange"},
        {Marc::CoreNormalHatch, "green"},
        {Marc::CoreContourHatch, "purple"},
        {Marc::HollowShell1OverhangHatch, "pink"},
        {Marc::HollowShell1NormalHatch, "cyan"},
        {Marc::HollowShell1ContourHatch, "magenta"},
        {Marc::HollowShell2OverhangHatch, "brown"},
        {Marc::HollowShell2NormalHatch, "gray"},
        {Marc::HollowShell2ContourHatch, "lime"},
        {Marc::SupportContourVolume, "navy"},
        {Marc::SupportHatch, "olive"},
        {Marc::PointSequence, "teal"},
        {Marc::ExternalSupports, "red"},
        {Marc::CoreContourHatchOverhang, "coral"},
        {Marc::HollowShell1ContourHatchOverhang, "indigo"},
        {Marc::HollowShell2ContourHatchOverhang, "violet"}
    };
    
    auto it = segment_colors.find(type);
    return it != segment_colors.end() ? it->second : "blue"; // Default to white if not found
}

void SlmPrint::readBuildStyles(const std::filesystem::path& stylesfile_json) 
{
    try {
       // std::cout << "StylesFilesPath: \"" << stylesfile_json.string() << "\"" << std::endl;
        
        ConfigurationManager config;
        if (config.loadFromFile(stylesfile_json)) {
           // std::cout << "Successfully loaded " << config.buildStyles.size() << " build styles." << std::endl;
            this->buildStyles = config.getStyles();  // Gives the styles back for the further processing..
        } else {
            std::cerr << "Warning: Failed to load build styles from " << stylesfile_json.string() << std::endl;
            // Could initialize with default styles here if needed
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in readBuildStyles: " << e.what() << std::endl;
        // Fallback mechanism - ensure buildStyles vector is not empty
        if (this->buildStyles.empty()) {
            std::cerr << "Using default build style as fallback." << std::endl;
            BuildStyle defaultStyle;
            defaultStyle.id = 1;
            defaultStyle.name = "Default";
            defaultStyle.description = "Default style created as fallback";
            defaultStyle.layerThickness = 0.03;
            this->buildStyles.push_back(defaultStyle);
        }
    }
}

void SlmPrint::readSlmConfig(const std::filesystem::path& configfile_json) 
{
    try {
        //std::cout << "SlmConfig Path: \"" << configfile_json.string() << "\"" << std::endl;
        
        // Use the Marc::SlmConfigReader we previously defined
        if (Marc::SlmConfigReader::loadFromFile(configfile_json.string(), this->jconfig)) {
           // std::cout << "Successfully loaded SlmConfig from " << configfile_json.string() << std::endl;
        } else {
            std::cerr << "Warning: Failed to load SlmConfig from " << configfile_json.string() << std::endl;
            std::cerr << "Using default SlmConfig values." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in readSlmConfig: " << e.what() << std::endl;
        std::cerr << "Using default SlmConfig values." << std::endl;
    }
}
// Add to SlmPrint class in SlmPrint.hpp

void SlmPrint::apply_all_configs() {
    try {
        // Apply the loaded SlmConfig to the underlying Print object
        this->print().apply_slm_config(this->jconfig);
        
        std::cout << "Applied SlmConfig from SlmPrint to Print structure" << std::endl;
        
        // For BuildStyles, we need additional logic to map them to specific regions or features
        if (!buildStyles.empty()) {
            std::cout << "Setting up " << buildStyles.size() << " build styles for future processing" << std::endl;
            
            // Here you might create a mapping between build styles and specific 
            // print regions or surface types for use during processing
            // This depends on your specific requirements
        }
    } catch (const std::exception& e) {
        std::cerr << "Error applying configuration in SlmPrint: " << e.what() << std::endl;
    }
}
//------------------------------------------------------------------Writing to *.marcfile

void SlmPrint::to_marc(const std::string& outputDir)  
{
    try {
        // Get the print object
        const Slic3r::Print& print = this->print();
        
        // Get the first print object
        if (print.objects.empty() || print.objects[0] == nullptr) {
            std::cerr << "Error: No print object found for export to MARC" << std::endl;
            return;
        }
        
        const Slic3r::PrintObject* printObject = print.objects[0];
        
        // Initialize the Marc file
        //Marc::MarcFile marcFile;
        std::unique_ptr<Marc::MarcFile> marcFile = std::make_unique<Marc::MarcFile>();
       // marcFile.initialize();
         //std::filesystem::path svgDir = std::filesystem::path(outputDir) / "SvgLayers";
   /// std::filesystem::create_directories(svgDir);
        // Determine the output directory and filename
       // std::string outputDir = this->config().output_directory_value;
     // Create a path object
std::filesystem::path outputDirPath = outputDir;

// Use the / operator to append the filename
std::filesystem::path marcFilePath = outputDirPath / "slicefile.marc";

// When printing, ensure consistent separators by using string() or u8string()
std::string filename = marcFilePath.string();

std::cout << "Exporting MARC file to: " << filename << std::endl;
        
        // Process each layer
        marcFile->addLayers(printObject);  // `addLayers` should handle the conversion of layers to MARC format
       

        // Finalize and write the file
        if (marcFile->writeToFile(filename)) {
            std::cout << "MARC file written successfully" << std::endl;
        } else {
            std::cerr << "Failed to write MARC file" << std::endl;
        }
    } 
    catch (const std::exception& e) {
        std::cerr << "Exception during MARC export: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error during MARC export" << std::endl;
    }
}



void SlmPrint::from_marc(const std::string& inputDir)
{
    try {
        // Construct the full path to the MARC file
        std::filesystem::path inputPath = std::filesystem::path(inputDir) / "slicefile.marc";
        std::string filename = inputPath.string();
        
        // Create a MarcFile object to handle reading
        Marc::MarcFile marcFile;

        // Read the MARC file
        if (marcFile.readFromFile(filename)) {
            std::cout << "MARC file read successfully from: " << filename << std::endl;
            
            // Process the imported data
           // processImportedLayers(marcFile);
        } else {
            std::cerr << "Failed to read MARC file: " << filename << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception during MARC import: " << e.what() << std::endl;
    }
    catch (...) {
        std::cerr << "Unknown error during MARC import" << std::endl;
    }
}
} // namespace Marc
// End of SlmPrint.cpp