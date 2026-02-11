# MarcSLM Developer Onboarding Checklist

Use this checklist to ensure your development environment is properly configured.

---

## ?? Phase 1: Environment Setup

### Prerequisites Installation
- [ ] **Visual Studio 2022** installed
  - [ ] "Desktop development with C++" workload selected
  - [ ] MSVC v143 toolset installed
  - [ ] Windows 10/11 SDK installed

- [ ] **CMake** installed (version 3.21+)
  - [ ] Added to system PATH
  - [ ] Verify: `cmake --version` in PowerShell

- [ ] **Git** installed
  - [ ] Configured with name and email
  - [ ] Verify: `git --version`

### vcpkg Setup
- [ ] Clone vcpkg repository
  ```powershell
  cd C:\
  git clone https://github.com/Microsoft/vcpkg.git
  cd vcpkg
  ```

- [ ] Bootstrap vcpkg
  ```powershell
  .\bootstrap-vcpkg.bat
  ```

- [ ] Integrate with Visual Studio
  ```powershell
  .\vcpkg integrate install
  ```

- [ ] Set environment variable
  ```powershell
  [Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
  ```
  - [ ] Restart PowerShell/terminal after setting

- [ ] Verify vcpkg integration
  ```powershell
  .\vcpkg version
  ```

---

## ?? Phase 2: Project Familiarization

### Documentation Review
- [ ] Read `README.md` (project overview)
- [ ] Read `QUICKSTART.md` (build instructions)
- [ ] Read `PROJECT_STRUCTURE.md` (architecture)
- [ ] Read `DELIVERY_SUMMARY.md` (comprehensive guide)

### Code Exploration
- [ ] Open `include/MarcSLM/Core/Types.hpp`
  - [ ] Review `Slice` structure
  - [ ] Understand coordinate conversion functions
  - [ ] Study `BuildStyleID` enum

- [ ] Open `include/MarcSLM/Core/Config.hpp`
  - [ ] Read rationale for design decisions
  - [ ] Note performance targets

- [ ] Open `tests/test_types.cpp`
  - [ ] Review test structure
  - [ ] Understand testing patterns

---

## ?? Phase 3: First Build

### Using CMake Presets (Recommended)
- [ ] Navigate to project root
  ```powershell
  cd C:\Active_Projects\Slicer-Core
  ```

- [ ] Configure project (Release)
  ```powershell
  cmake --preset windows-x64-release
  ```
  ?? **Expected time**: 15-30 minutes (first build, downloading dependencies)

- [ ] Build project
  ```powershell
  cmake --build --preset windows-x64-release
  ```

- [ ] Run unit tests
  ```powershell
  ctest --preset windows-x64-release --output-on-failure
  ```
  ? **Expected**: All 20+ tests passing

### Verify Build Artifacts
- [ ] Check library exists
  ```powershell
  Test-Path out\build\x64-Release\lib\MarcSLM_Core.lib
  ```

- [ ] Check test executable exists
  ```powershell
  Test-Path out\build\x64-Release\bin\MarcSLM_Tests.exe
  ```

### Alternative: Visual Studio GUI
- [ ] Open Visual Studio 2022
- [ ] File ? Open ? CMake...
- [ ] Select `CMakeLists.txt`
- [ ] Wait for CMake configuration (automatic)
- [ ] Select "windows-x64-release" from dropdown
- [ ] Build ? Build All (Ctrl+Shift+B)
- [ ] Test ? Run All Tests

---

## ?? Phase 4: Development Workflow

### IDE Configuration
- [ ] **Visual Studio**
  - [ ] Solution Explorer: Switch to "CMake Targets View"
  - [ ] Enable "Show All Files" in Solution Explorer
  - [ ] Set default configuration to "windows-x64-release"

- [ ] **VS Code** (Alternative)
  - [ ] Install "C/C++" extension
  - [ ] Install "CMake Tools" extension
  - [ ] Configure CMake Kit: "Visual Studio Community 2022 - amd64"

### Code Style Guidelines
- [ ] Review inline code comments in `Types.hpp`
- [ ] Note use of:
  - [ ] `noexcept` for performance-critical functions
  - [ ] `const` correctness throughout
  - [ ] Move semantics (no copy constructors for large objects)
  - [ ] Doxygen-style documentation (`///`, `@brief`, `@param`)

### Git Workflow
- [ ] Review `.gitignore` (ensures build artifacts aren't committed)
- [ ] Create feature branch
  ```powershell
  git checkout -b feature/your-feature-name
  ```

---

## ?? Phase 5: Your First Contribution

### Example Task: Add a Helper Function
Let's add a simple utility function to practice the workflow.

#### Step 1: Understand the Task
- [ ] Goal: Add a function to calculate slice area (placeholder implementation)

#### Step 2: Modify Header
- [ ] Open `include/MarcSLM/Core/Types.hpp`
- [ ] Add function declaration after `Slice` struct:
  ```cpp
  /// @brief Calculate the approximate area of a slice (placeholder)
  /// @param slice Slice to measure
  /// @return Approximate area in square millimeters
  double calculateSliceArea(const Slice& slice);
  ```

#### Step 3: Create Implementation
- [ ] Create `src/Core/Types.cpp`
- [ ] Add implementation:
  ```cpp
  #include <MarcSLM/Core/Types.hpp>
  
  namespace MarcSLM {
  namespace Core {
  
  double calculateSliceArea(const Slice& slice) {
      // Placeholder: return 0 for now (will implement with Clipper2 later)
      return 0.0;
  }
  
  } // namespace Core
  } // namespace MarcSLM
  ```

#### Step 4: Update CMakeLists.txt
- [ ] Open `CMakeLists.txt`
- [ ] Add to `MARCSLM_SOURCES`:
  ```cmake
  set(MARCSLM_SOURCES
      src/Core/Types.cpp
  )
  ```

#### Step 5: Write Unit Test
- [ ] Open `tests/test_types.cpp`
- [ ] Add test at the end:
  ```cpp
  TEST(SliceTest, AreaCalculation) {
      Slice slice(1.0, 0);
      slice.outerContour = {
          Point2DInt(0, 0),
          Point2DInt(10000000, 0),
          Point2DInt(10000000, 10000000),
          Point2DInt(0, 10000000)
      };
      
      double area = calculateSliceArea(slice);
      // For now, placeholder returns 0
      EXPECT_EQ(area, 0.0);
  }
  ```

#### Step 6: Build and Test
- [ ] Rebuild project
  ```powershell
  cmake --build --preset windows-x64-release
  ```

- [ ] Run tests
  ```powershell
  ctest --preset windows-x64-release --output-on-failure
  ```

- [ ] Verify new test passes

#### Step 7: Commit Changes
- [ ] Stage changes
  ```powershell
  git add include/MarcSLM/Core/Types.hpp
  git add src/Core/Types.cpp
  git add tests/test_types.cpp
  git add CMakeLists.txt
  ```

- [ ] Commit with descriptive message
  ```powershell
  git commit -m "Add calculateSliceArea placeholder function with unit test"
  ```

---

## ?? Phase 6: Advanced Development

### Debugging
- [ ] Set breakpoint in Visual Studio (F9)
- [ ] Debug ? Debug Tests ? Select test to debug
- [ ] Step through code (F10/F11)

### Performance Profiling
- [ ] Build with RelWithDebInfo configuration
- [ ] Use Visual Studio Profiler (Alt+F2)
- [ ] Analyze ? Performance Profiler ? CPU Usage

### Code Coverage (Optional)
- [ ] Install OpenCppCoverage (Windows)
- [ ] Run tests with coverage
  ```powershell
  OpenCppCoverage.exe --sources include\MarcSLM `
    -- .\out\build\x64-Release\bin\MarcSLM_Tests.exe
  ```

---

## ?? Phase 7: Module Development

When you're ready to implement a full module:

### Geometry Module Example
- [ ] Create `include/MarcSLM/Geometry/MeshLoader.hpp`
- [ ] Add public API design (classes, functions)
- [ ] Create `src/Geometry/MeshLoader.cpp`
- [ ] Implement core functionality
- [ ] Add to `MARCSLM_GEOMETRY_HEADERS` in CMakeLists.txt
- [ ] Add to `MARCSLM_SOURCES` in CMakeLists.txt
- [ ] Create `tests/test_geometry.cpp`
- [ ] Write comprehensive unit tests
- [ ] Update module README.md with implementation details

---

## ?? Milestone Checklist

### Beginner (Onboarded)
- [ ] Environment fully configured
- [ ] First successful build
- [ ] All tests passing
- [ ] First commit made

### Intermediate (Contributing)
- [ ] Implemented helper function
- [ ] Written unit tests
- [ ] Debugged code in Visual Studio
- [ ] Reviewed pull request

### Advanced (Module Owner)
- [ ] Designed full module API
- [ ] Implemented core functionality
- [ ] Achieved >80% test coverage
- [ ] Profiled performance

---

## ?? Quick Reference Commands

```powershell
# Configure
cmake --preset windows-x64-release

# Build
cmake --build --preset windows-x64-release

# Test
ctest --preset windows-x64-release -V

# Clean build
rm -r out\build\x64-Release
cmake --preset windows-x64-release

# Run specific test
.\out\build\x64-Release\bin\MarcSLM_Tests.exe --gtest_filter=SliceTest.*

# List all tests
.\out\build\x64-Release\bin\MarcSLM_Tests.exe --gtest_list_tests
```

---

## ?? Troubleshooting

### Issue: "VCPKG_ROOT not found"
- [ ] Verify environment variable: `echo $env:VCPKG_ROOT`
- [ ] Restart terminal after setting
- [ ] Set manually in current session: `$env:VCPKG_ROOT = "C:\vcpkg"`

### Issue: CMake configuration fails
- [ ] Clear cache: `rm out\build\x64-Release\CMakeCache.txt`
- [ ] Reconfigure: `cmake --preset windows-x64-release`

### Issue: Link errors with dependencies
- [ ] Clean vcpkg cache: `vcpkg remove --outdated`
- [ ] Rebuild dependencies: `vcpkg install --triplet x64-windows`

### Issue: Tests fail
- [ ] Check test output: `ctest --preset windows-x64-release --output-on-failure`
- [ ] Run single test: `.\out\build\x64-Release\bin\MarcSLM_Tests.exe --gtest_filter=TestName`

---

## ? Completion

**You are ready to contribute when:**
- [ ] All checkboxes in Phases 1-4 are completed
- [ ] First build succeeded with all tests passing
- [ ] You've made at least one successful code change (Phase 5)
- [ ] You understand the project structure and workflow

**Welcome to the MarcSLM development team! ??**

---

**Last Updated**: 2024  
**Maintainer**: MarcSLM Core Team
