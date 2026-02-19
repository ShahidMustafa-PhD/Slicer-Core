// ==============================================================================
// MarcSLM - Mesh I/O Integration Example
// ==============================================================================
// Example code showing how to integrate the new Mesh I/O module with the
// existing MarcSLM pipeline (MeshProcessor + Manifold).
// ==============================================================================

#pragma once

#include "MarcSLM/IO/MeshIO.hpp"
#include "MarcSLM/IO/MeshData.hpp"
#include <manifold/manifold.h>
#include <memory>

namespace MarcSLM {
namespace IO {

/// @brief Convert MeshData (Eigen matrices) to Manifold::MeshGL format.
/// @param mesh Input mesh data with Eigen matrices
/// @return Manifold::MeshGL structure ready for slicing
/// @note This is a bridge function to integrate data-oriented MeshIO
///       with the existing geometry processing pipeline.
inline manifold::MeshGL eigenToManifoldMesh(const MeshData& mesh) {
    manifold::MeshGL result;
    
    // Reserve memory for efficiency
    const size_t numVertices = mesh.vertexCount();
    const size_t numFaces = mesh.faceCount();
    
    result.vertProperties.reserve(numVertices * 3);
    result.triVerts.reserve(numFaces * 3);
    
    // Set number of properties per vertex (just positions: x,y,z)
    result.numProp = 3;
    
    // Copy vertex positions (Eigen::MatrixXd -> std::vector<float>)
    for (int i = 0; i < mesh.V.rows(); ++i) {
        result.vertProperties.push_back(static_cast<float>(mesh.V(i, 0)));  // X
        result.vertProperties.push_back(static_cast<float>(mesh.V(i, 1)));  // Y
        result.vertProperties.push_back(static_cast<float>(mesh.V(i, 2)));  // Z
    }
    
    // Copy face indices (Eigen::MatrixXi -> std::vector<uint32_t>)
    for (int i = 0; i < mesh.F.rows(); ++i) {
        result.triVerts.push_back(static_cast<uint32_t>(mesh.F(i, 0)));
        result.triVerts.push_back(static_cast<uint32_t>(mesh.F(i, 1)));
        result.triVerts.push_back(static_cast<uint32_t>(mesh.F(i, 2)));
    }
    
    return result;
}

/// @brief Load mesh file and convert to Manifold for slicing pipeline.
/// @param filePath Path to 3D model file (STL, OBJ, AMF, etc.)
/// @return Unique pointer to Manifold solid ready for slicing
/// @throws MeshLoadError if file cannot be loaded
/// @throws NonManifoldMeshError if topology validation fails
/// 
/// @par Example Usage:
/// @code
///     auto solid = loadAndConvertToManifold("turbine_blade.stl");
///     auto layers = processor.sliceUniform(solid.get(), 0.05f);
/// @endcode
inline std::unique_ptr<manifold::Manifold> 
loadAndConvertToManifold(const std::string& filePath) {
    // Step 1: Load with data-oriented MeshIO
    MeshIO loader;
    MeshData meshData;
    
    if (!loader.loadFromFile(filePath, meshData)) {
        throw MeshLoadError(filePath, loader.lastError());
    }
    
    // Step 2: Convert Eigen matrices to Manifold format
    manifold::MeshGL manifoldMesh = eigenToManifoldMesh(meshData);
    
    // Step 3: Create Manifold solid
    auto manifoldPtr = std::make_unique<manifold::Manifold>(manifoldMesh);
    
    // Step 4: Validate Manifold status
    manifold::Manifold::Error status = manifoldPtr->Status();
    if (status != manifold::Manifold::Error::NoError) {
        // Convert Manifold error to string
        std::string errorMsg;
        switch (status) {
            case manifold::Manifold::Error::NonFiniteVertex:
                errorMsg = "Non-finite vertex coordinates (NaN/Inf)";
                break;
            case manifold::Manifold::Error::NotManifold:
                errorMsg = "Mesh is not manifold (topology error)";
                break;
            case manifold::Manifold::Error::VertexOutOfBounds:
                errorMsg = "Vertex index out of bounds";
                break;
            case manifold::Manifold::Error::PropertiesWrongLength:
                errorMsg = "Properties array has wrong length";
                break;
            default:
                errorMsg = "Unknown Manifold error";
        }
        throw MeshValidationError(errorMsg);
    }
    
    return manifoldPtr;
}

} // namespace IO
} // namespace MarcSLM

// ==============================================================================
// Example Integration with Existing Pipeline
// ==============================================================================

#if 0  // Example code (not compiled)

#include "MarcSLM/IO/MeshIO.hpp"
#include "MarcSLM/Geometry/MeshProcessor.hpp"
#include "MarcSLM/SlmPrint.hpp"

void processPartWithNewIO(const std::string& partPath, 
                          const MarcSLM::SlmConfig& config) {
    try {
        // === OPTION 1: Direct Manifold Loading (Recommended) ===
        
        // Load mesh with new I/O module
        auto manifold = MarcSLM::IO::loadAndConvertToManifold(partPath);
        
        // Use with existing MeshProcessor
        MarcSLM::Geometry::MeshProcessor processor;
        processor.setManifold(std::move(manifold));  // If API supports this
        
        // Slice as normal
        auto layers = processor.sliceUniform(config.layer_thickness);
        
        // === OPTION 2: Full Pipeline with SlmPrint ===
        
        MarcSLM::SlmPrint engine;
        engine.setSlmConfig(config);
        
        // Load mesh using new I/O
        if (!engine.loadMeshWithIO(partPath)) {  // If modified to use new I/O
            std::cerr << "Failed to load mesh\n";
            return;
        }
        
        // Process as normal
        engine.slice();
        engine.generatePaths();
        auto classified = engine.classify();
        engine.exportMarc("output/part.marc");
        
    } catch (const MarcSLM::IO::MeshLoadError& e) {
        std::cerr << "Load failed: " << e.what() << "\n";
        std::cerr << "File: " << e.filePath() << "\n";
        std::cerr << "Reason: " << e.reason() << "\n";
        
    } catch (const MarcSLM::IO::NonManifoldMeshError& e) {
        std::cerr << "Non-manifold mesh: " << e.what() << "\n";
        std::cerr << "Problematic edges: " << e.nonManifoldEdgeCount() << "\n";
        std::cerr << "Consider using mesh repair tools.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
}

// === OPTION 3: Analyze Mesh Before Processing ===

void analyzeMeshWithNewIO(const std::string& partPath) {
    MarcSLM::IO::MeshIO loader;
    MarcSLM::IO::MeshData mesh;
    
    if (!loader.loadFromFile(partPath, mesh)) {
        std::cerr << "Failed to load: " << loader.lastError() << "\n";
        return;
    }
    
    // Print mesh statistics
    std::cout << "Mesh Analysis:\n";
    std::cout << "  Vertices:    " << mesh.vertexCount() << "\n";
    std::cout << "  Faces:       " << mesh.faceCount() << "\n";
    
    auto dims = mesh.dimensions();
    std::cout << "  Dimensions:  " << dims(0) << " x " 
              << dims(1) << " x " << dims(2) << " mm\n";
    
    auto bbox = mesh.boundingBox();
    std::cout << "  Bounding Box:\n";
    std::cout << "    Min: (" << bbox(0) << ", " << bbox(1) << ", " << bbox(2) << ")\n";
    std::cout << "    Max: (" << bbox(3) << ", " << bbox(4) << ", " << bbox(5) << ")\n";
    
    // Validate manifoldness
    try {
        loader.validateManifold(mesh);
        std::cout << "  Manifold:    ? Yes\n";
    } catch (const MarcSLM::IO::NonManifoldMeshError& e) {
        std::cout << "  Manifold:    ? No (" << e.nonManifoldEdgeCount() << " issues)\n";
    }
    
    // Estimate layer count for slicing
    double layerThickness = 0.05;  // 50 microns
    double height = dims(2);
    size_t estimatedLayers = static_cast<size_t>(std::ceil(height / layerThickness));
    std::cout << "  Est. Layers: " << estimatedLayers 
              << " (at " << layerThickness * 1000 << " µm)\n";
}

#endif // Example code
