# MarcSLM - Quick Start Guide

## Prerequisites

1. **Visual Studio 2022** (Community, Professional, or Enterprise)
   - Install "Desktop development with C++" workload
   - Ensure MSVC v143 toolset is installed

2. **CMake 3.21 or later**
   - Download from https://cmake.org/download/
   - Add to PATH during installation

3. **vcpkg** (Package Manager)
   ```powershell
   # Clone vcpkg
   cd C:\
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   
   # Bootstrap
   .\bootstrap-vcpkg.bat
   
   # Integrate with Visual Studio
   .\vcpkg integrate install
   ```

4. **Set Environment Variable**
   ```powershell
   # Set permanently
   [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
   
   # Or set for current session
   $env:VCPKG_ROOT = "C:\vcpkg"
   ```

## Step-by-Step Build Instructions

### Option 1: Using CMake Presets (Recommended)

```powershell
# Navigate to project root
cd C:\Active_Projects\Slicer-Core

# Configure (vcpkg will automatically download dependencies)
cmake --preset windows-x64-release

# Build
cmake --build --preset windows-x64-release

# Run tests
ctest --preset windows-x64-release --output-on-failure
```

### Option 2: Manual CMake Configuration

```powershell
# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"

# Build
cmake --build . --config Release

# Run tests
ctest -C Release --output-on-failure
```

### Option 3: Visual Studio GUI

1. Open Visual Studio 2022
2. File ? Open ? CMake... ? Select `CMakeLists.txt`
3. Visual Studio will automatically detect `CMakePresets.json`
4. Select configuration from dropdown (e.g., `windows-x64-release`)
5. Build ? Build All
6. Test ? Run All Tests

## First-Time Build Notes

- **vcpkg dependency download**: The first build will take 15-30 minutes as vcpkg downloads and compiles all dependencies (assimp, manifold, clipper2, glm, tbb, gtest)
- **Subsequent builds**: Much faster (incremental compilation only)
- **Dependencies are cached**: vcpkg caches compiled libraries in `vcpkg_installed/` directory

## Verify Installation

After successful build, verify the output:

```powershell
# Check library output
dir out\build\x64-Release\lib\MarcSLM_Core.lib

# Check test executable
dir out\build\x64-Release\bin\MarcSLM_Tests.exe

# Run tests manually
.\out\build\x64-Release\bin\MarcSLM_Tests.exe --gtest_list_tests
```

## Project Structure Overview

```
C:\Active_Projects\Slicer-Core\
?
??? include/MarcSLM/          # Public headers
?   ??? Core/                 # Primitive types, .marc format
?   ??? Geometry/             # 3D ? 2D conversion bridge
?   ??? Thermal/              # BuildStyle classification
?   ??? PathPlanning/         # Scan vector generation
?
??? src/                      # Implementation files
?   ??? Core/
?   ??? Geometry/
?   ??? Thermal/
?   ??? PathPlanning/
?
??? tests/                    # Google Test unit tests
?   ??? test_types.cpp        # Core types tests
?
??? cmake/                    # CMake modules
??? out/build/                # Build output (auto-generated)
??? vcpkg_installed/          # Dependency cache (auto-generated)
?
??? CMakeLists.txt            # Root build configuration
??? CMakePresets.json         # Build presets for Visual Studio
??? vcpkg.json                # Dependency manifest
```

## Common Issues

### Issue: "VCPKG_ROOT not found"
**Solution**: Ensure environment variable is set and restart terminal/Visual Studio

### Issue: CMake can't find dependencies
**Solution**: 
```powershell
# Clear CMake cache
rm -r out/build
rm -r vcpkg_installed

# Reconfigure
cmake --preset windows-x64-release
```

### Issue: Build fails with "permission denied"
**Solution**: Close Visual Studio and any processes locking files, then rebuild

### Issue: vcpkg fails to download packages
**Solution**: Check internet connection and firewall settings

## Next Steps

1. **Review Core Types**: Open `include/MarcSLM/Core/Types.hpp`
2. **Run Example**: See `README.md` for usage examples
3. **Add Features**: Start implementing geometry bridge in `include/MarcSLM/Geometry/`
4. **Write Tests**: Add tests in `tests/` directory

## Support

For build issues or questions:
- Check `README.md` for detailed documentation
- Review CMake configuration in `CMakeLists.txt`
- Inspect vcpkg manifest in `vcpkg.json`

---

**Happy Building! ??**
