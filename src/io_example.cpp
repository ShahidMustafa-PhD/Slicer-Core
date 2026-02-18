// ==============================================================================
// MarcSLM - Mesh I/O CLI Example
// ==============================================================================
// Minimal command-line tool to test MeshIO functionality.
// Loads a 3D mesh and prints statistics.
// ==============================================================================
// Usage: io_example <mesh_file.stl>
// ==============================================================================

#include "MarcSLM/IO/MeshIO.hpp"
#include "MarcSLM/IO/MeshData.hpp"

#include <iostream>
#include <iomanip>
#include <cstdlib>

void printUsage(const char* programName) {
    std::cout << "========================================\n";
    std::cout << "  MarcSLM - Mesh I/O Example\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    std::cout << "Usage:\n";
    std::cout << "  " << programName << " <mesh_file>\n";
    std::cout << "\n";
    std::cout << "Supported Formats:\n";
    std::cout << "  STL (Binary/ASCII), OBJ, AMF, 3MF, PLY, FBX\n";
    std::cout << "\n";
    std::cout << "Example:\n";
    std::cout << "  " << programName << " part.stl\n";
    std::cout << std::endl;
}

void printMeshStatistics(const MarcSLM::IO::MeshData& mesh) {
    std::cout << "\n========================================\n";
    std::cout << "  Mesh Statistics\n";
    std::cout << "========================================\n";
    std::cout << "\n";
    
    // Basic counts
    std::cout << std::left << std::setw(25) << "Source File:"
              << mesh.sourcePath << "\n";
    std::cout << std::setw(25) << "Format:"
              << mesh.formatHint << "\n";
    std::cout << std::setw(25) << "Vertex Count:"
              << mesh.vertexCount() << "\n";
    std::cout << std::setw(25) << "Face Count:"
              << mesh.faceCount() << "\n";
    
    // Bounding box
    Eigen::RowVector<double, 6> bbox = mesh.boundingBox();
    std::cout << "\nBounding Box [mm]:\n";
    std::cout << std::setw(25) << "  Min (X, Y, Z):"
              << "(" << std::fixed << std::setprecision(3)
              << bbox(0) << ", " << bbox(1) << ", " << bbox(2) << ")\n";
    std::cout << std::setw(25) << "  Max (X, Y, Z):"
              << "(" << bbox(3) << ", " << bbox(4) << ", " << bbox(5) << ")\n";
    
    // Dimensions
    Eigen::Vector3d dims = mesh.dimensions();
    std::cout << "\nDimensions [mm]:\n";
    std::cout << std::setw(25) << "  Width (X):" << dims(0) << "\n";
    std::cout << std::setw(25) << "  Depth (Y):" << dims(1) << "\n";
    std::cout << std::setw(25) << "  Height (Z):" << dims(2) << "\n";
    
    // Volume estimation (bounding box approximation)
    double bboxVolume = dims(0) * dims(1) * dims(2);
    std::cout << "\n" << std::setw(25) << "Bounding Box Volume:" 
              << bboxVolume << " mm³\n";
    std::cout << std::setw(25) << "" 
              << bboxVolume / 1000.0 << " cm³\n";
    
    std::cout << "\n========================================\n";
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    if (argc < 2) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }
    
    std::string filePath = argv[1];
    
    // Check for help flag
    if (filePath == "-h" || filePath == "--help") {
        printUsage(argv[0]);
        return EXIT_SUCCESS;
    }
    
    std::cout << "Loading mesh: " << filePath << "\n";
    std::cout << "Please wait...\n" << std::flush;
    
    try {
        // Create MeshIO loader
        MarcSLM::IO::MeshIO loader;
        
        // Load mesh data
        MarcSLM::IO::MeshData mesh;
        bool success = loader.loadFromFile(filePath, mesh);
        
        if (!success) {
            std::cerr << "\nError: " << loader.lastError() << "\n";
            return EXIT_FAILURE;
        }
        
        // Print statistics
        printMeshStatistics(mesh);
        
        // Check for validation warnings
        if (!loader.lastError().empty()) {
            std::cout << "\n? Warning: " << loader.lastError() << "\n";
        }
        
        return EXIT_SUCCESS;
        
    } catch (const MarcSLM::IO::MeshLoadError& e) {
        std::cerr << "\n? Load Error: " << e.what() << "\n";
        std::cerr << "  File: " << e.filePath() << "\n";
        std::cerr << "  Reason: " << e.reason() << "\n";
        return EXIT_FAILURE;
        
    } catch (const MarcSLM::IO::NonManifoldMeshError& e) {
        std::cerr << "\n? Manifold Error: " << e.what() << "\n";
        std::cerr << "  Non-manifold edges: " << e.nonManifoldEdgeCount() << "\n";
        std::cerr << "\n  Note: Some SLM slicers can repair non-manifold meshes.\n";
        std::cerr << "        Consider using mesh repair tools (e.g., Meshmixer, Netfabb).\n";
        return EXIT_FAILURE;
        
    } catch (const MarcSLM::IO::MeshValidationError& e) {
        std::cerr << "\n? Validation Error: " << e.what() << "\n";
        std::cerr << "  Details: " << e.details() << "\n";
        return EXIT_FAILURE;
        
    } catch (const MarcSLM::IO::MeshIOError& e) {
        std::cerr << "\n? I/O Error: " << e.what() << "\n";
        return EXIT_FAILURE;
        
    } catch (const std::exception& e) {
        std::cerr << "\n? Unexpected Error: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
