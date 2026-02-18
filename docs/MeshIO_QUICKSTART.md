# Quick Start Guide - Mesh I/O Module

## Installation

### 1. Install Eigen3 via vcpkg

```bash
# Navigate to your vcpkg installation
cd C:/vcpkg  # Adjust path as needed

# Install Eigen3
./vcpkg install eigen3:x64-windows

# Verify installation
./vcpkg list eigen3
```

### 2. Reconfigure CMake

```bash
# Navigate to project root
cd C:/Active_Projects/Slicer-Core

# Reconfigure with updated vcpkg manifest
cmake --preset=default

# Or manually:
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 3. Build the Project

```bash
# Build all targets
cmake --build build --config Release

# Or build specific targets:
cmake --build build --target io_example --config Release
cmake --build build --target MarcSLM_Tests --config Release
```

## Quick Test

### Test the CLI Tool

```bash
# Navigate to build output
cd build/bin

# Run with any STL file
./io_example.exe ../../samples/cube.stl

# Expected output:
========================================
  Mesh Statistics
========================================

Source File:             ../../samples/cube.stl
Format:                  stl
Vertex Count:            8
Face Count:              12

Bounding Box [mm]:
  Min (X, Y, Z):         (0.000, 0.000, 0.000)
  Max (X, Y, Z):         (10.000, 10.000, 10.000)

Dimensions [mm]:
  Width (X):             10.000
  Depth (Y):             10.000
  Height (Z):            10.000

Bounding Box Volume:     1000.000 mm³
                         1.000 cm³

========================================
```

### Run Unit Tests

```bash
cd build

# Run all tests
ctest --output-on-failure

# Or run specific test executable
./bin/MarcSLM_Tests.exe --gtest_filter="MeshIO*"
```

## Integration with Existing Code

### Replace MeshProcessor Loading

```cpp
// OLD (MeshProcessor with Assimp)
MarcSLM::Geometry::MeshProcessor processor;
auto mesh = processor.loadMesh("part.stl");

// NEW (MeshIO with Eigen)
MarcSLM::IO::MeshIO loader;
MarcSLM::IO::MeshData meshData;
loader.loadFromFile("part.stl", meshData);

// Convert to Manifold
manifold::MeshGL manifoldMesh;
manifoldMesh.vertPos.reserve(meshData.vertexCount() * 3);
for (int i = 0; i < meshData.V.rows(); ++i) {
    manifoldMesh.vertPos.push_back(meshData.V(i, 0));
    manifoldMesh.vertPos.push_back(meshData.V(i, 1));
    manifoldMesh.vertPos.push_back(meshData.V(i, 2));
}

manifoldMesh.triVerts.reserve(meshData.faceCount() * 3);
for (int i = 0; i < meshData.F.rows(); ++i) {
    manifoldMesh.triVerts.push_back(meshData.F(i, 0));
    manifoldMesh.triVerts.push_back(meshData.F(i, 1));
    manifoldMesh.triVerts.push_back(meshData.F(i, 2));
}

auto manifold = manifold::Manifold(manifoldMesh);
```

## Troubleshooting

### Issue: "CMake Error: find_package(Eigen3) not found"

**Solution**: Install Eigen3 via vcpkg:
```bash
vcpkg install eigen3:x64-windows
```

### Issue: "Cannot open include file 'Eigen/Core'"

**Solution**: Ensure vcpkg toolchain is specified:
```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Issue: "Non-manifold mesh detected"

**Solution**: Use mesh repair tools:
- **Netfabb** (Commercial)
- **Meshmixer** (Free from Autodesk)
- **MeshLab** (Open-source)

Or disable strict validation in code:
```cpp
try {
    loader.loadFromFile("part.stl", mesh);
} catch (const MarcSLM::IO::NonManifoldMeshError& e) {
    // Log warning but continue
    std::cerr << "Warning: " << e.what() << "\n";
    // mesh is still loaded
}
```

## Next Steps

1. ? Install Eigen3
2. ? Build project
3. ? Run tests
4. ? Test CLI tool
5. ?? Integrate with main pipeline
6. ?? Add Eigen ? Manifold bridge function
7. ?? Update documentation

## Support

For issues or questions:
- Check `docs/MeshIO_README.md` for detailed documentation
- Run tests with: `ctest --verbose`
- Check build logs: `build/CMakeFiles/CMakeOutput.log`
