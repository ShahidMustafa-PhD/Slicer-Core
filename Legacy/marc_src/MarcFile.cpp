#include "MarcFile.hpp"
#include <filesystem> // Required for std::filesystem

namespace fs = std::filesystem;

namespace Marc {
// Write binary .marc file
bool MarcFile::writeToFile(const std::string& filename) 
{
    std::ofstream out(filename, std::ios::binary);    // Open file in binary mode
    if (!out.is_open()) {
        std::cerr << "Error opening file for writing." << std::endl;
        return false;                 // Return false if cannot open
    }

    // Set total number of layers in the header
    header.totalLayers = static_cast<uint32_t>(m_layers.size()); 
    std::cout << "PrintObject has " << m_layers.size() << " layers" << std::endl;
    std::cout << "MarcFile has " << m_layers.size() << " layers" << std::endl;
    
    // Write header first (indexTableOffset is initially zero)
    out.write(reinterpret_cast<const char*>(&header), sizeof(header)); 
    std::cout << "Header written, indexTableOffset: " << header.indexTableOffset << std::endl;

    indexTable.clear(); // Clear previous index table
    // Iterate over layers and write layer data to file
    for (const auto& layer : m_layers) 
    {
        indexTable.push_back(static_cast<uint64_t>(out.tellp()));  // Save offset of this layer
       // std::cout << "Layer " << layer.layerNumber << " offset: " << out.tellp() << std::endl;

        // Write layer number and height
        out.write(reinterpret_cast<const char*>(&layer.layerNumber), sizeof(layer.layerNumber));
        out.write(reinterpret_cast<const char*>(&layer.layerHeight), sizeof(layer.layerHeight));

        // Write hatches
        uint32_t hatchCount = static_cast<uint32_t>(layer.hatches.size());
        out.write(reinterpret_cast<const char*>(&hatchCount), sizeof(hatchCount));
        for (const auto& hatch : layer.hatches) {
            out.write(reinterpret_cast<const char*>(&hatch.tag), sizeof(hatch.tag));
            for (const auto& line : hatch.lines) {
                writePoint(out, line.a);
                writePoint(out, line.b);
            }
        }

        // Write polylines
        uint32_t polylineCount = static_cast<uint32_t>(layer.polylines.size());
        out.write(reinterpret_cast<const char*>(&polylineCount), sizeof(polylineCount));
        for (const auto& polyline : layer.polylines) {
            out.write(reinterpret_cast<const char*>(&polyline.tag), sizeof(polyline.tag));
            for (const auto& pt : polyline.points) writePoint(out, pt);
        }

        // Write polygons
        uint32_t polygonCount = static_cast<uint32_t>(layer.polygons.size());
        out.write(reinterpret_cast<const char*>(&polygonCount), sizeof(polygonCount));
        for (const auto& polygon : layer.polygons) {
            out.write(reinterpret_cast<const char*>(&polygon.tag), sizeof(polygon.tag));
            for (const auto& pt : polygon.points) writePoint(out, pt);
        }
    }

    // Write index table after layer data
    header.indexTableOffset = static_cast<uint64_t>(out.tellp());
    //std::cout << "Index Table Offset: " << header.indexTableOffset << std::endl;

    for (auto offset : indexTable) {
        out.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    // Rewrite updated header with correct index offset
    out.seekp(0); // Seek back to the beginning to update the header
    out.write(reinterpret_cast<const char*>(&header), sizeof(header));

    out.close();
    return true;
}

// Read binary .marc file
bool MarcFile::readFromFile(const std::string& filename) 
{
    std::ifstream in(filename, std::ios::binary);    // Open file for reading
    if (!in.is_open()) {
        std::cerr << "Error opening file for reading." << std::endl;
        return false;                 // Return false if open failed
    }

    // Read header first
    in.read(reinterpret_cast<char*>(&header), sizeof(header));  
    if (strncmp(header.magic, "MARC", 4) != 0) {
        std::cerr << "Invalid file format (wrong magic number)." << std::endl;
        return false;
    }

    std::cout << "Header read, indexTableOffset: " << header.indexTableOffset << std::endl;

    // Read index table
    in.seekg(header.indexTableOffset);
    indexTable.resize(header.totalLayers);
    for (uint32_t i = 0; i < header.totalLayers; ++i) {
        in.read(reinterpret_cast<char*>(&indexTable[i]), sizeof(uint64_t));
        //std::cout << "Layer " << i + 1 << " indexTable offset: " << indexTable[i] << std::endl;
    }

    // Read each layer
    m_layers.resize(header.totalLayers);
    for (uint32_t i = 0; i < header.totalLayers; ++i) {
        in.seekg(indexTable[i]);  // Seek to the layer offset
        Layer& layer = m_layers[i];
        in.read(reinterpret_cast<char*>(&layer.layerNumber), sizeof(layer.layerNumber));
        in.read(reinterpret_cast<char*>(&layer.layerHeight), sizeof(layer.layerHeight));
       // std::cout << "Test1_Layer " << layer.layerNumber  << std::endl;
        // Read hatches
        uint32_t hatchCount;
        in.read(reinterpret_cast<char*>(&hatchCount), sizeof(hatchCount));
        layer.hatches.resize(hatchCount);
        for (auto& hatch : layer.hatches) {
            in.read(reinterpret_cast<char*>(&hatch.tag), sizeof(hatch.tag));
            uint32_t lineCount = hatch.tag.pointCount / 2;
            hatch.lines.resize(lineCount);
            for (auto& line : hatch.lines) {
                readPoint(in, line.a);
                readPoint(in, line.b);
            }
        }
       // std::cout << "Test2 "  << std::endl;
        // Read polylines
        uint32_t polylineCount;
        in.read(reinterpret_cast<char*>(&polylineCount), sizeof(polylineCount));
        
       // std::cout << "polylinecount  "  << polylineCount<<std::endl;
        
        layer.polylines.resize(polylineCount);
        for (auto& polyline : layer.polylines) {
            in.read(reinterpret_cast<char*>(&polyline.tag), sizeof(polyline.tag));
        
            
            
            polyline.points.resize(polyline.tag.pointCount);
            for (auto& pt : polyline.points) readPoint(in, pt);
        }
       // std::cout << "Test3 "  << std::endl;
        // Read polygons
        uint32_t polygonCount;
        in.read(reinterpret_cast<char*>(&polygonCount), sizeof(polygonCount));
        layer.polygons.resize(polygonCount);
        for (auto& polygon : layer.polygons) {
            //std::cout << "Test4 "  << std::endl;
            in.read(reinterpret_cast<char*>(&polygon.tag), sizeof(polygon.tag));
            polygon.points.resize(polygon.tag.pointCount);
            for (auto& pt : polygon.points) readPoint(in, pt);
        }
        //std::cout << "Test5 "  << std::endl;
    }

    in.close();

    //----Write SVG for each layer. This is the output
     this->generateSvg(filename);// Generate SVG files from the MARC file
    return true;
}
int MarcFile::generateSvg( std::string filename)
 {
    // Create MarcFile object and read the contents of a .marc file
    //MarcFile marcFile;
    //std::string filename = "test.marc";  // Replace with your actual file path
    std::string outputDir = "../output";  // Output directory to save SVGs

    generateSVGForMarcFile(outputDir) ;

    return 0;
}

// Function to iterate through the layers and generate SVG for each layer
void MarcFile::generateSVGForMarcFile(const std::string& outputDir) 
{
    fs::create_directories(outputDir);  // Ensure output directory exists

    for (const auto& layer : m_layers) {
        std::string outputPath = outputDir + "/layer" + std::to_string(layer.layerNumber) + ".svg";
        generateSVGForLayer(layer, outputPath);
    }
}



std::string escapeXML(const std::string& text) {
    std::ostringstream out;
    for (char c : text) {
        switch (c) {
            case '&': out << "&amp;"; break;
            case '<': out << "&lt;"; break;
            case '>': out << "&gt;"; break;
            case '"': out << "&quot;"; break;
            case '\'': out << "&apos;"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

void MarcFile::generateSVGForLayer(const Layer& layer, const std::string& outputPath) {
   // Create a new marcToSVG object
   Marc::marcToSVG svgGenerator(outputPath.c_str());
       // Draw basic elements
       Point origin; // Set origin for SVG
       origin.x = -100;
       origin.y = -100;
        svgGenerator.origin = origin; // Set the origin for the SVG
        svgGenerator.draw(origin, "red", 0.1);
        svgGenerator.draw(origin, "none", 100);
   // Draw all hatches, polylines, and polygons
   for (const auto& hatch : layer.hatches) {
       svgGenerator.draw(hatch.lines,"green",0.1); // hatch.lines should be Marc::Lines (std::vector<Marc::Line>)
   }
   for (const auto& polyline : layer.polylines) {
       svgGenerator.draw(polyline,"black",0.1);
   }
   for (const auto& polygon : layer.polygons) {
       svgGenerator.draw(polygon,"red",0.1);
   }

   // Close the SVG file
   svgGenerator.Close();
}

void Marc::MarcFile::initialize() {
    // Clear all internal data structures to prepare for a new export session
    m_layers.clear();
    
    // Initialize the header with default values
    MarcHeader newHeader;
    
    // Set the magic number (already set in the struct initialization)
    // newHeader.magic is already set to {'M','A','R','C'}
    
    // Set version (already set to 1 in the struct)
    // newHeader.version = 1;
    
    // Reset layer count (already set to 0 in the struct)
    // newHeader.totalLayers = 0;
    
    // Reset index table offset (already set to 0 in the struct)
    // newHeader.indexTableOffset = 0;
    
    // Update timestamp to current time (already set in the struct initialization)
    // newHeader.timestamp = std::time(nullptr);
    
    // Clear printer ID (already zeroed in the struct initialization)
    // memset(newHeader.printerId, 0, sizeof(newHeader.printerId));
    
    // Clear reserved space (already zeroed in the struct initialization)
    // memset(newHeader.reserved, 0, sizeof(newHeader.reserved));
    
    // Store the initialized header
    this->header = newHeader;
    
   // std::cout << "MARC file initialized with timestamp: " 
              //<< this->header.timestamp << std::endl;
}

/**
 * @brief Extracts all layer geometry data from a PrintObject and populates m_layers.
 * 
 * This method processes all layers and regions within a PrintObject, extracting:
 * - Hatches (scan segments with lines)
 * - Perimeters (extrusion loops)
 * - Thin fills (infill extrusion paths)
 * - Support surfaces (polygons for support structures)
 * - Other extrusion entities (bridges, solid infills, etc.)
 * 
 * Data is converted to Marc format (points, lines, polylines, polygons) and
 * stored in m_layers for binary file serialization.
 * 
 * @param printObject Pointer to Slic3r::PrintObject containing layer data
 */
void Marc::MarcFile::addLayers(const Slic3r::PrintObject* printObject) {
    if (!printObject) {
        std::cerr << "Error: Attempted to add layers from null PrintObject" << std::endl;
        return;
    }

    // Create geometry converter for transforming Slic3r types to Marc types
    Marc::Point origin;
    origin.x = 0.0;
    origin.y = 0.0;
    auto converter = std::make_unique<Marc::ExportMarcSlice>(origin);

    // Reserve space for all layers to improve performance
    this->m_layers.reserve(printObject->layers.size());

    // Process each layer in the print object
    for (const Slic3r::Layer* layer : printObject->layers) {
        if (!layer) continue;

        // Create a new Marc::Layer for this layer
        Marc::Layer marcLayer;
        
        // Set basic layer properties
        marcLayer.layerHeight = layer->print_z;          // Top surface Z position
        marcLayer.layerThickness = layer->height;       // Layer thickness
        marcLayer.layerNumber = layer->id();             // Unique layer ID

        // Process all regions and their geometry
        processLayerRegions(layer, marcLayer, converter);

        // Add the completed layer to collection
        m_layers.push_back(marcLayer);
    }

    // Process support layers if they exist
    for (const Slic3r::Layer* support_layer : printObject->support_layers) {
        if (!support_layer) continue;

        Marc::Layer marcLayer;
        marcLayer.layerHeight = support_layer->print_z;
        marcLayer.layerThickness = support_layer->height;
        marcLayer.layerNumber = support_layer->id() + 10000;  // Offset ID for support layers

        processLayerRegions(support_layer, marcLayer, converter);
        m_layers.push_back(marcLayer);
    }

    std::cout << "Successfully added " << m_layers.size() << " layers (normal + support)" << std::endl;
}

/**
 * @brief Processes all regions within a layer and extracts their geometry.
 * 
 * @param layer The Slic3r layer to process
 * @param marcLayer Output Marc layer to populate
 * @param converter Geometry converter for Slic3r to Marc type conversion
 */
void Marc::MarcFile::processLayerRegions(const Slic3r::Layer* layer, Layer& marcLayer,
                                          std::unique_ptr<ExportMarcSlice>& converter) {
    if (!layer) return;

    // Iterate through each region in the layer
    for (const Slic3r::LayerRegion* region : layer->regions) {
        if (!region) continue;

        // Process hatches (primary scan lines for SLM)
        processHatches(region->hatches, marcLayer, converter);

        // Process perimeter extrusions (boundary contours)
        if (!region->perimeters.entities.empty()) {
            processExtrusionEntities(region->perimeters.entities, marcLayer, converter);
        }

        // Process thin wall fills (thin fill geometry)
        if (!region->thin_fills.entities.empty()) {
            processExtrusionEntities(region->thin_fills.entities, marcLayer, converter);
        }

        // Process support surfaces (support structure polygons)
        processSurfaceCollection(region->support_surfaces, marcLayer, converter);
    }
}

/**
 * @brief Extracts hatch data (scan line segments) from a region.
 * 
 * Hatches represent the primary laser scan paths in SLM processing.
 * Each hatch consists of multiple line segments.
 * 
 * @param hatches Vector of SlmHatch objects
 * @param marcLayer Output layer to populate
 * @param converter Geometry converter for Slic3r to Marc type conversion
 */
void Marc::MarcFile::processHatches(const std::vector<SlmHatch>& hatches, Layer& marcLayer,
                                     std::unique_ptr<ExportMarcSlice>& converter) {
    for (const SlmHatch& hatch : hatches) {
        // Convert Slic3r lines to Marc lines
        std::vector<Marc::Line> m_lines;
        converter->convrt(hatch.lines(), m_lines);

        // Create Marc hatch structure
        Marc::Hatch m_hatch(GeometryType::CoreHatch);  // Type-safe construction
        m_hatch.tag.pointCount = static_cast<uint32_t>(m_lines.size() * 2);  // 2 points per line
        m_hatch.lines = std::move(m_lines);  // Move semantics for efficiency

        marcLayer.hatches.push_back(std::move(m_hatch));
    }
}

/**
 * @brief Processes extrusion entities (paths, loops, collections) polymorphically.
 * 
 * Handles the recursive nature of ExtrusionEntityCollection. Extracts polylines
 * from extrusion paths and loops, converting them for output.
 * 
 * @param entities Vector of extrusion entity pointers
 * @param marcLayer Output layer to populate
 * @param converter Geometry converter
 */
void Marc::MarcFile::processExtrusionEntities(const Slic3r::ExtrusionEntitiesPtr& entities, 
                                               Layer& marcLayer,
                                               std::unique_ptr<ExportMarcSlice>& converter) {
    for (const Slic3r::ExtrusionEntity* entity : entities) {
        processExtrusionEntity(entity, marcLayer, converter);
    }
}

/**
 * @brief Recursively processes a single extrusion entity.
 * 
 * Handles three polymorphic types:
 * - ExtrusionPath: Single continuous polyline
 * - ExtrusionLoop: Closed loop of paths
 * - ExtrusionEntityCollection: Nested collection (recursive)
 * 
 * @param entity Pointer to extrusion entity
 * @param marcLayer Output layer to populate
 * @param converter Geometry converter
 */
void Marc::MarcFile::processExtrusionEntity(const Slic3r::ExtrusionEntity* entity, Layer& marcLayer,
                                             std::unique_ptr<ExportMarcSlice>& converter) {
    if (!entity || !converter) return;

    // Handle ExtrusionPath (single polyline segment)
    if (const Slic3r::ExtrusionPath* path = dynamic_cast<const Slic3r::ExtrusionPath*>(entity)) {
        Marc::Polyline m_polyline(GeometryType::Perimeter);  // Type-safe construction
        converter->convrt(path->polyline, m_polyline);
        m_polyline.tag.pointCount = static_cast<uint32_t>(m_polyline.points.size());
        marcLayer.polylines.push_back(std::move(m_polyline));
    }
    // Handle ExtrusionLoop (closed contour)
    else if (const Slic3r::ExtrusionLoop* loop = dynamic_cast<const Slic3r::ExtrusionLoop*>(entity)) {
        // Process each path within the loop
        for (const Slic3r::ExtrusionPath& path : loop->paths) {
            Marc::Polyline m_polyline(GeometryType::Perimeter);
            converter->convrt(path.polyline, m_polyline);
            m_polyline.tag.pointCount = static_cast<uint32_t>(m_polyline.points.size());
            marcLayer.polylines.push_back(std::move(m_polyline));
        }
    }
    // Handle ExtrusionEntityCollection (recursive call)
    else if (const Slic3r::ExtrusionEntityCollection* collection = 
             dynamic_cast<const Slic3r::ExtrusionEntityCollection*>(entity)) {
        processExtrusionEntities(collection->entities, marcLayer, converter);
    }
}

/**
 * @brief Processes support surfaces and converts them to polygons.
 * 
 * Support surfaces represent the structure needed to support overhanging features.
 * Extracts expolygon data (contours + holes) and stores as Marc polygons.
 * 
 * @param surfaces SurfaceCollection containing support geometry
 * @param marcLayer Output layer to populate
 * @param converter Geometry converter
 */
void Marc::MarcFile::processSurfaceCollection(const Slic3r::SurfaceCollection& surfaces, Layer& marcLayer,
                                               std::unique_ptr<ExportMarcSlice>& converter) {
    if (surfaces.surfaces.empty()) return;

    for (const Slic3r::Surface& surface : surfaces.surfaces) {
        // Get the expolygon (polygon with holes) from surface
        const Slic3r::ExPolygon& expolygon = surface.expolygon;

        // Convert contour (outer boundary)
        Slic3r::Polygon contour_poly = expolygon.contour;
        Marc::Polygon m_contour_poly(GeometryType::SupportStructure);  // Type-safe construction
        converter->convrt(contour_poly, m_contour_poly);
        m_contour_poly.tag.pointCount = static_cast<uint32_t>(m_contour_poly.points.size());
        marcLayer.polygons.push_back(std::move(m_contour_poly));

        // Convert holes (inner boundaries)
        for (const Slic3r::Polygon& hole : expolygon.holes) {
            Marc::Polygon m_hole_poly(GeometryType::SupportStructure);
            converter->convrt(hole, m_hole_poly);
            m_hole_poly.tag.pointCount = static_cast<uint32_t>(m_hole_poly.points.size());
            marcLayer.polygons.push_back(std::move(m_hole_poly));
        }
    }
}





/*
void MarcFile::write_sla_svg(const Slic3r::SLAPrint* sla_print)  // Writes original slaprint data
{   //sla_print->doSomething();  // assuming SLAPrint has a method doSomething()
        //std::cout << "writingnow\n" << std::endl ;
// ----data structure of in  SLA layer----
//ExPolygonCollection slices;// Just  ExPolygons expolygons;
//ExPolygonCollection perimeters;
//ExtrusionEntityCollection infill;
//ExPolygonCollection solid_infill;
//float slice_z, print_z;
//bool solid;
Slic3r::Pointf origin;
//BoundingBoxf3 bb=sla_print->bb;
   // origin.x= bb.min.x;
   // origin.y= bb.min.y;
int i=0;
//FOREACH_LAYER(object, layer)
for (const auto& layerm :sla_print->layers)
{  
    std::string outputfile("Layer");
    std::ostringstream num;
    num << ++i;
  // outputfile=outputfile+num.str();
  //const char *ch= outputfile.c_str();
  //sv->layer_section_start(ch);
   Marc::SVG *sv  = new SVG(outputfile+num.str()+std::string(".svg")); // allocate memory
   sv->draw( Slic3r::Point(0,0 ), std::string("red"), scale_(0.1));
   sv->draw( Slic3r::Point(0,0 ), std::string("none"), scale_(80));
    
 

  

//-------------------------------------------------------------------------

Slic3r::ExtrusionEntityCollection m_fills = layerm.infill.flatten(1);
Slic3r::ExtrusionEntitiesPtr entities = m_fills.entities;

for (Slic3r::ExtrusionEntitiesPtr::const_iterator it = entities.begin(); it != entities.end(); ++it) {
    Slic3r::ExtrusionPath* path = dynamic_cast<Slic3r::ExtrusionPath*>(*it);
    if (path) {
        // Translate the path
        Slic3r::Polyline translated_path = path->polyline;
        //translated_path.translate(object->_copies_shift);

        // Draw the points from the translated polyline
        sv->draw(translated_path, role_color(*it), 0.1);
    }
}



 

//__________________________________________________________________
//ExPolygonCollection slices   expolygons
  //for (auto surface =layerm->slices.surfaces.begin(); surface != layerm->slices.surfaces.end(); ++surface) 
  for (const auto& expolygon :layerm.slices)
  {
    
       // Slic3r::ExPolygon expp= surface->expolygon;
        Slic3r::Polyline translated_path = expolygon.contour;
        //translated_path.translate(object->_copies_shift);
        sv->draw(translated_path,"green" , 0.2);
        Slic3r::Polygons polys=expolygon.holes;
        for (const auto& poly : polys)
        {  translated_path=poly;
           // translated_path.translate(object->_copies_shift);
           sv->draw(translated_path, "green", 0.2);
        }
	
    }

//____________________________________________________________________
//ExPolygonCollection perimeters;
for (const auto& expolygon :layerm.perimeters)
  {
    
       // Slic3r::ExPolygon expp= surface->expolygon;
        Slic3r::Polyline translated_path = expolygon.contour;
        //translated_path.translate(object->_copies_shift);
        sv->draw(translated_path,"red" , 0.2);
        Slic3r::Polygons polys=expolygon.holes;
        for (const auto& poly : polys)
        {  translated_path=poly;
           // translated_path.translate(object->_copies_shift);
           sv->draw(translated_path, "red", 0.2);
        }
	
    }



//-----------------------------------------------------------------------------
//ExPolygonCollection solid_infill;

for (const auto& expolygon :layerm.solid_infill)
  {
    
       // Slic3r::ExPolygon expp= surface->expolygon;
        Slic3r::Polyline translated_path = expolygon.contour;
        //translated_path.translate(object->_copies_shift);
        //sv->draw(translated_path,"red" , 0.2);
        Slic3r::Polygons polys=expolygon.holes;
        for (const auto& poly : polys)
        {  translated_path=poly;
           // translated_path.translate(object->_copies_shift);
           //sv->draw(translated_path, "red", 0.2);
        }
	
    }


//______________________________________________________________________________
//PolylineCollection unsupported_bridge_edges;

//______________________________________________________________________________

    //sv->layer_section_end();
    delete  sv;
    //std::cout << "writingEnd\n" << std::endl ;
    
    }//Each Layer
}//scope


*/

std::string MarcFile::role_color(Slic3r::ExtrusionEntity *entity) const
{
    if (entity == nullptr) {
        return "white"; // or handle error appropriately
    }
    
    if (entity->is_perimeter()) return "black";
    if (entity->is_infill()) return "red";
    if (entity->is_solid_infill()) return "green";  // corrected typo
    // if (entity->is_bridge()) return "blue";
    
    return "darkmagenta";
}

std::string MarcFile::surface_color(Slic3r::Surface* surface) const
{
    if (surface == nullptr) {
        return "white"; // or handle error appropriately
    }
    Slic3r::SurfaceType  type=surface->surface_type;
    //stTop            = 0b1,       /// stTop: it has nothing just on top of it
    //stBottom         = 0b10,      /// stBottom: it's a surface with nothing just under it (or the base plate, or a support)
    //stInternal       = 0b100,     /// stInternal: not top nor bottom
    //stSolid          = 0b1000,    /// stSolid: modify the stInternal to say it should be at 100% infill
    //stBridge         = 0b10000,   /// stBridge: modify stBottom or stInternal to say it should be extruded as a bridge
    //stVoid           = 0b100000 
    if(type==stTop) return "bisque";
    if(type==stBottom) return "darkorange";
    if(type==stInternal) return "blueviolet";
    if(type==stSolid) return "chartreuse";
    if(type==stBridge) return "darkcyan";
    if(type==stVoid) return "black";

    
    return "darkmagenta";
}





} // namespace Marc

