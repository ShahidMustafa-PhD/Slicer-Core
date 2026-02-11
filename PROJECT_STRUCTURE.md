# MarcSLM Project Structure

```
MarcSLM/
?
??? ?? CMakeLists.txt                    # Root build configuration
??? ?? CMakePresets.json                 # Visual Studio build presets
??? ?? vcpkg.json                        # Dependency manifest
??? ?? README.md                         # Project overview and usage
??? ?? QUICKSTART.md                     # Step-by-step build guide
??? ?? .gitignore                        # Git ignore rules
?
??? ?? cmake/                            # CMake configuration
?   ??? MarcSLM_CoreConfig.cmake.in     # Package config template
?   ??? Modules/                         # Custom CMake modules
?       ??? README.md
?
??? ?? include/MarcSLM/                  # Public API Headers
?   ??? Core/                            # ? Primitive types & .marc format
?   ?   ??? Types.hpp                    # Slice, Point2D/3D, BuildStyleID
?   ?   ??? Config.hpp                   # Project configuration & documentation
?   ?
?   ??? Geometry/                        # ?? 3D?2D conversion bridge
?   ?   ??? README.md                    # (Assimp + Manifold + Clipper2)
?   ?
?   ??? Thermal/                         # ??? BuildStyle classification
?   ?   ??? README.md                    # (Contour, Skin, Infill, Support)
?   ?
?   ??? PathPlanning/                    # ?? Scan vector generation
?       ??? README.md                    # (Hatching, stripe decomposition)
?
??? ?? src/                              # Implementation Files
?   ??? Core/                            # Core implementations
?   ?   ??? README.md
?   ??? Geometry/                        # Geometry implementations
?   ?   ??? README.md
?   ??? Thermal/                         # Thermal implementations
?   ?   ??? README.md
?   ??? PathPlanning/                    # PathPlanning implementations
?       ??? README.md
?
??? ?? tests/                            # Unit Tests (Google Test)
?   ??? CMakeLists.txt                   # Test build configuration
?   ??? test_types.cpp                   # ? Core types unit tests
?   ??? README.md                        # Testing documentation
?
??? ?? out/build/                        # ??? Build output (auto-generated)
?   ??? x64-Debug/                       # Debug build artifacts
?   ??? x64-Release/                     # Release build artifacts
?       ??? lib/                         # MarcSLM_Core.lib
?       ??? bin/                         # MarcSLM_Tests.exe
?
??? ?? vcpkg_installed/                  # ?? Dependencies cache (auto-generated)
    ??? x64-windows/                     # Compiled libraries
        ??? include/                     # assimp, manifold, clipper2, glm, tbb
        ??? lib/                         # Static/dynamic libraries

```

## Module Responsibilities

### ? **Core** (`include/MarcSLM/Core/`)
- **Types.hpp**: Fundamental data structures
  - `Slice`: 2D layer with contour and holes
  - `Point2D` / `Point3D`: Coordinate primitives
  - `BuildStyleID`: Thermal region classification enum
  - `.marc` file format definitions
  - Coordinate conversion utilities (mm ? Clipper2)

- **Config.hpp**: Project-wide configuration
  - Build settings documentation
  - Performance targets
  - Dependency rationale

### ?? **Geometry** (`include/MarcSLM/Geometry/`)
*To be implemented*
- Mesh loading via Assimp
- 3D slicing via Manifold
- Contour extraction to Clipper2 paths

### ??? **Thermal** (`include/MarcSLM/Thermal/`)
*To be implemented*
- Region classification (Contour, Skin, Infill, Support)
- Layer-to-layer thermal analysis
- BuildStyleID mapping

### ?? **PathPlanning** (`include/MarcSLM/PathPlanning/`)
*To be implemented*
- Hatching pattern generation
- Scan vector optimization
- Stripe decomposition for thermal control

## Build Artifacts

| File | Location | Description |
|------|----------|-------------|
| `MarcSLM_Core.lib` | `out/build/x64-Release/lib/` | Static library |
| `MarcSLM_Tests.exe` | `out/build/x64-Release/bin/` | Unit test executable |
| Dependency DLLs | `out/build/x64-Release/bin/` | Runtime libraries (if dynamic) |

## Key Configuration Files

| File | Purpose |
|------|---------|
| `vcpkg.json` | Declares dependencies (assimp, manifold, etc.) |
| `CMakeLists.txt` | Build system configuration |
| `CMakePresets.json` | Visual Studio integration presets |
| `cmake/MarcSLM_CoreConfig.cmake.in` | Package config for downstream projects |

## Development Workflow

1. **Read**: Start with `QUICKSTART.md` for build instructions
2. **Explore**: Review `include/MarcSLM/Core/Types.hpp` for API design
3. **Build**: Use `cmake --preset windows-x64-release`
4. **Test**: Run `ctest --preset windows-x64-release`
5. **Extend**: Add new headers in `include/MarcSLM/[Module]/`
6. **Implement**: Add `.cpp` files in `src/[Module]/`
7. **Validate**: Write tests in `tests/test_[module].cpp`

## Status Legend

- ? Implemented and tested
- ?? In progress
- ? Planned
- ?? Documentation
- ??? Auto-generated

---

**Current Status**: Core infrastructure complete, ready for geometry module development.
