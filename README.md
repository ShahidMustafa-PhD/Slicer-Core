# MarcSLM - High-Performance Industrial DMLS/SLM Slicer Core

## Overview

MarcSLM is a modern, high-performance C++ geometry engine designed for industrial Direct Metal Laser Sintering (DMLS) and Selective Laser Melting (SLM) applications. It replaces legacy AGPL-licensed engines with a commercial-friendly stack built on industry-standard libraries.

## Architecture

### Core Dependencies

- **Manifold**: Robust 3D mesh slicing and boolean operations
- **Clipper2**: High-performance 2D polygon clipping and offsetting
- **Assimp**: Universal 3D model import/export
- **GLM**: OpenGL Mathematics for vector/matrix operations
- **Intel TBB**: Parallel algorithms for multi-core geometry processing

### Module Structure

```
MarcSLM/
??? Core/          - Primitive types (Point, Line, Slice, .marc format)
??? Geometry/      - 3D mesh ? 2D slice conversion bridge
??? Thermal/       - BuildStyle region classification
??? PathPlanning/  - Scan vector and hatching algorithms
```

## Build Requirements

- **Platform**: Windows x64
- **Compiler**: MSVC 2022 (Visual Studio 17) or later
- **CMake**: 3.21 or later
- **vcpkg**: For dependency management

## Building the Project

### 1. Install vcpkg

```powershell
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

Set the `VCPKG_ROOT` environment variable:
```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
```

### 2. Configure and Build

Using CMake Presets (recommended):

```powershell
# Debug build
cmake --preset windows-x64-debug
cmake --build --preset windows-x64-debug

# Release build
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

Manual configuration:

```powershell
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### 3. Run Tests

```powershell
ctest --preset windows-x64-release
```

## Usage Example

```cpp
#include <MarcSLM/Core/Types.hpp>

using namespace MarcSLM::Core;

// Create a slice at 0.5mm height
Slice slice(0.5, /* layerIndex */ 10);

// Define outer contour (square 10mm x 10mm)
slice.outerContour = {
    {mmToClipperUnits(0.0), mmToClipperUnits(0.0)},
    {mmToClipperUnits(10.0), mmToClipperUnits(0.0)},
    {mmToClipperUnits(10.0), mmToClipperUnits(10.0)},
    {mmToClipperUnits(0.0), mmToClipperUnits(10.0)}
};

// Add a circular hole (simplified as octagon)
Clipper2Lib::Path64 hole;
// ... populate hole vertices ...
slice.holes.push_back(std::move(hole));
```

## License

This project is licensed under a commercial-friendly license (non-AGPL). See LICENSE file for details.

## Project Status

?? **Active Development** - Core geometry engine is under construction.

### Completed
- ? Build system configuration
- ? Core primitive types
- ? Clipper2 integration

### In Progress
- ?? Manifold 3D slicing bridge
- ?? Assimp model loading
- ?? Thermal region classification

### Planned
- ? Scan vector generation
- ? Hatching algorithms
- ? .marc binary format serialization

## Contributing

This is a proprietary project. Contributions are managed internally.

## Contact

For inquiries, please contact the MarcSLM development team.
