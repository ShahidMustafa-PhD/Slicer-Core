#pragma once
// marc_file.h

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>
#include <array>
#include <ctime>
#include <cstring>
#include <iostream>


#include <sstream>
#include <iomanip>  // for setprecision
#include <filesystem>

#include <limits>
#include <algorithm>
#include <string>
//#include "../libslic3r/SLAPrint.hpp"

#include "../libslic3r/libslic3r.h"
#include "../libslic3r/ExtrusionEntity.hpp"
#include "../libslic3r/ExtrusionEntityCollection.hpp"
#include "../libslic3r/Print.hpp"
#include "ExportMarcSlice.hpp"
#include "SVG.hpp"// Slm SVG class Modified.....
#include <iostream>
#include <memory>
#include <cstdio>
#include "marcToSVG.hpp"
//#include <chrono>



namespace Marc {


// -------------------------------
// MARC File Class
// -------------------------------
class MarcFile {
public:
    MarcHeader header;                         // File header
    std::vector<Layer> m_layers;                 // All layers in file
    std::vector<uint64_t> indexTable;          // Index table with offsets to each layer
    bool writeToFile(const std::string& filename);
    bool readFromFile(const std::string& filename);
    int generateSvg( std::string filename);
    void initialize();  // Initialize the file structure
    void addLayers(const Slic3r::PrintObject* printObject);
private:
    // Helper: write a point to file
    void writePoint(std::ofstream& out, const Point& p) {
        out.write(reinterpret_cast<const char*>(&p), sizeof(p));
    }

    // Helper: read a point from file
    void readPoint(std::ifstream& in, Point& p) {
        in.read(reinterpret_cast<char*>(&p), sizeof(p));
    }
    void generateSVGForMarcFile(const std::string& outputDir);
    void generateSVGForLayer(const Layer& layer, const std::string& outputPath);
    
    std::string role_color(Slic3r::ExtrusionEntity *entity) const;
    std::string surface_color(Slic3r::Surface* surface) const;
    
    // Helper methods for addLayers to extract geometry data
    void processLayerRegions(const Slic3r::Layer* layer, Layer& marcLayer, 
                             std::unique_ptr<ExportMarcSlice>& converter);
    void processHatches(const std::vector<SlmHatch>& hatches, Layer& marcLayer,
                        std::unique_ptr<ExportMarcSlice>& converter);
    void processExtrusionEntities(const Slic3r::ExtrusionEntitiesPtr& entities, Layer& marcLayer,
                                  std::unique_ptr<ExportMarcSlice>& converter);
    void processExtrusionEntity(const Slic3r::ExtrusionEntity* entity, Layer& marcLayer,
                                std::unique_ptr<ExportMarcSlice>& converter);
    void processSurfaceCollection(const Slic3r::SurfaceCollection& surfaces, Layer& marcLayer,
                                  std::unique_ptr<ExportMarcSlice>& converter);
};


} // namespace Marc

/*
    Binary File Format (.marc):
    -----------------------------------
    - Header (160 bytes):
      - magic[4]: "MARC"
      - version[4]: file format version
      - totalLayers[4]: total number of layers
      - indexTableOffset[8]: file offset where index table begins
      - timestamp[8]: Unix timestamp
      - printerId[32]: string identifying the printer
      - reserved[100]: for future use

    - Layer Data Blocks:
      For each layer:
        - layerNumber[4]
        - layerHeight[4]
        - hatchCount[4], then list of hatches
          - each hatch: type[1], lineCount[4], then lines
        - polylineCount[4], then list of polylines
          - each polyline: type[1], pointCount[4], then points
        - polygonCount[4], then list of polygons
          - each polygon: type[1], pointCount[4], then points

    - Index Table:
      - list of x uint64_t values, each indicating byte offset of corresponding layer
*/
   

// test_driver.cpp
/*
#include "marc_file_format.h"
#include <iostream>

int main() {
    MarcFile file;
    std::strncpy(file.header.printerId, "L-PBF_Machine_X", 32);

    Layer layer;
    layer.layerNumber = 1;
    layer.layerHeight = 0.05f;

    Hatch h;
    h.type = 1;
    h.lineCount = 1;
    h.lines.push_back({{0,0}, {10,0}});
    layer.hatches.push_back(h);
    layer.hatchCount = 1;

    Polyline pl;
    pl.type = 2;
    pl.pointCount = 4;
    pl.points = {{0,0}, {2,2}, {4,2}, {6,0}};
    layer.polylines.push_back(pl);
    layer.polylineCount = 1;

    Polygon pg;
    pg.type = 3;
    pg.pointCount = 6;
    pg.points = {{0,0}, {1,2}, {2,2}, {3,0}, {2,-2}, {1,-2}};
    layer.polygons.push_back(pg);
    layer.polygonCount = 1;

    file.layers.push_back(layer);

    file.writeToFile("test_output.marc");

    MarcFile loaded;
    if (loaded.readFromFile("test_output.marc")) {
        std::cout << "Read success: " << loaded.layers.size() << " layers\n";
        std::cout << "Printer ID: " << loaded.header.printerId << "\n";
        std::cout << "Timestamp: " << loaded.header.timestamp << "\n";
    } else {
        std::cerr << "Failed to read file.\n";
    }
    return 0;
}
*/


// Here id GPT Prompt
/*1. 
I want to develop structure of a binary file *.marc.
It saves the data for a L-PBF based metal 3D printer machine. 
My data  is  x number of layers. 
Each layer has layer number, layer hight, number of bytes used to store current layer data.
The layer data is  g number of hatches , and  k number of  polylines, and i number of  polygons.
Each line has 2 points p(x,y) to save.  
Each hatch  have m number of lines and the polylines and polygons may have n number of points. 
where n is an even integer. In a layer, each hatch, or polyline or polygon can be one of 16 different types. 
Therefore tag of either of these will have a number 0-15. 
The header of the file has all the  important information like total number of layers and address of  a location,
where addreses of all  layer in the file is saved in sequence 1 to x.  
The header has 160 byte size. make the  data structure of such a binary  file.  
The file should be clearly defined so that it can be easily read back without loosing any information. 
Also generate  a c++ class to write and read this file.
Also gine me clearly labeled data structure for my future reference and knowledge. 
Remember that it is a custom data structure to store a compact binary file,
which is used to communicate the layer slice data with a powder bed fusion based 3D metal powder printer. 
You can also take information from any commercial  L-PBF  metal 3D printer used in the industery. it is optional.
// Additional prompt After promp1.

2. yes generate the  C++ writeToFile and readFromFile functions implemented in full, 
including how each structure is serialized/deserialized. 
comment the code comprehensively.take care of memory safety. 
i donot want compression or encryption.
The code in c++ must be effecient and easy to understand.


3. I like:

A test driver to generate or read a .marc file.



Versioning support in the header.

Extended metadata (timestamp, printer ID, etc.).

4. Seperate the header file of the class from implementation. 
I have my CmakeLists.txt. no need to generate buildable project. 
Just give me a clean concise class which i will include in my  project.
Give me the documentation of file internal data structure.
give driver programe as well. 

*/

// file data Structure
/*| Offset | Size   | Description                        |
|--------|--------|------------------------------------|
| 0      | 160 B  | Header (versioning, timestamp, ID) |
| 160    | Var.   | Layer Data Blocks                  |
| Var.   | Var.   | Index Table (list of offsets)      |

Each Layer Block contains:

Layer number + height

Hatches: Count → [type, line count, lines...]

Polylines: Count → [type, point count, points...]

Polygons: Count → [type, point count, points...]
*/