// ==============================================================================
// MarcSLM - Project Configuration and Build Settings
// ==============================================================================
// This file documents the build configuration decisions and rationale
// ==============================================================================

/*
 * COMPILER CONFIGURATION
 * ======================
 * 
 * Standard: C++17
 * Rationale: 
 *   - Modern features (structured bindings, if constexpr, std::optional)
 *   - Broad compiler support (MSVC 2017+, GCC 7+, Clang 5+)
 *   - std::filesystem available (for future file I/O)
 * 
 * MSVC Flags:
 *   /utf-8           - UTF-8 source and execution character sets
 *   /W4              - High warning level (catch potential issues)
 *   /permissive-     - Strict standards conformance
 *   /O2              - Maximum optimization (Release)
 *   /GL              - Whole program optimization
 *   /LTCG            - Link-time code generation
 *   /MP              - Multi-processor compilation (parallel build)
 * 
 * Defines:
 *   _CRT_SECURE_NO_WARNINGS - Disable warnings for legacy C functions
 */

/*
 * DEPENDENCY VERSIONS
 * ===================
 * 
 * assimp >= 5.3.1
 *   - Model import/export (STL, 3MF, OBJ, etc.)
 *   - Used for: Loading 3D meshes before slicing
 * 
 * manifold >= 2.4.5
 *   - Robust 3D boolean operations and mesh slicing
 *   - Used for: Cutting meshes at Z-planes to generate 2D contours
 * 
 * clipper2 >= 1.3.0
 *   - High-performance 2D polygon clipping/offsetting
 *   - Used for: Contour offsetting, infill generation, boolean ops
 * 
 * glm >= 0.9.9.8
 *   - OpenGL Mathematics (vector/matrix operations)
 *   - Used for: 3D transformations, coordinate math
 * 
 * tbb >= 2021.10.0
 *   - Intel Threading Building Blocks
 *   - Used for: Parallel layer processing, multi-threaded geometry ops
 * 
 * gtest >= 1.14.0
 *   - Google Test framework
 *   - Used for: Unit testing, regression testing
 */

/*
 * COORDINATE SYSTEM CONVENTIONS
 * ==============================
 * 
 * 3D Space (Input Meshes):
 *   - Right-handed coordinate system
 *   - Z-axis: Build direction (vertical, upward)
 *   - XY-plane: Build plate
 *   - Units: Millimeters (floating-point)
 * 
 * 2D Slices (Clipper2):
 *   - Integer coordinate system (int64_t)
 *   - Scaling factor: 1e6 (1 unit = 1 nanometer)
 *   - Precision: Sub-micron accuracy
 *   - Orientation: Counter-clockwise (CCW) for outer contours
 *                  Clockwise (CW) for holes
 * 
 * Conversion:
 *   mm ? Clipper: multiply by 1,000,000
 *   Clipper ? mm: divide by 1,000,000
 */

/*
 * MEMORY MANAGEMENT STRATEGY
 * ===========================
 * 
 * Slicing Pipeline:
 *   - Move semantics for zero-copy transfers
 *   - std::unique_ptr for ownership transfer
 *   - std::vector for slice collections
 *   - No raw pointers in public API
 * 
 * Parallelization:
 *   - Thread-safe slice processing (immutable reads)
 *   - Intel TBB for parallel_for over layers
 *   - Lock-free data structures where possible
 * 
 * Large Meshes:
 *   - Stream processing for file I/O
 *   - Incremental slicing (layer-by-layer)
 *   - Memory pooling for temporary geometry (future optimization)
 */

/*
 * PERFORMANCE TARGETS
 * ===================
 * 
 * Slicing Speed:
 *   - Target: > 100 layers/second for typical industrial parts
 *   - Mesh size: 100k-1M triangles
 *   - Platform: Intel Core i7 (8+ cores)
 * 
 * Memory Usage:
 *   - < 2GB RAM for 10k layers with moderate complexity
 *   - Efficient memory reuse in pipeline
 * 
 * Precision:
 *   - Geometric accuracy: ±1 micron (10^-6 m)
 *   - Thermal zone boundaries: ±10 microns
 */

/*
 * BUILD SYSTEM ORGANIZATION
 * ==========================
 * 
 * CMake Structure:
 *   - Root CMakeLists.txt: Project-wide configuration
 *   - Modular subdirectories (tests/, future apps/)
 *   - Target-based design (MarcSLM::Core alias)
 * 
 * vcpkg Integration:
 *   - Manifest mode (vcpkg.json)
 *   - Locked dependency versions (builtin-baseline)
 *   - Automatic dependency resolution
 * 
 * Output Directories:
 *   - Libraries: out/build/[config]/lib/
 *   - Executables: out/build/[config]/bin/
 *   - Tests: Integrated with CTest
 */

/*
 * TESTING STRATEGY
 * =================
 * 
 * Unit Tests (Google Test):
 *   - Test all public APIs
 *   - Edge cases (empty slices, degenerate geometry)
 *   - Numerical precision validation
 * 
 * Integration Tests:
 *   - End-to-end slicing pipeline
 *   - Real-world STL file processing
 *   - .marc format serialization/deserialization
 * 
 * Performance Tests:
 *   - Benchmark critical paths
 *   - Regression detection (time/memory)
 *   - Profiler integration (future)
 */

/*
 * FUTURE EXTENSIONS
 * ==================
 * 
 * Planned Features:
 *   1. GPU-accelerated hatching (CUDA/OpenCL)
 *   2. Adaptive layer thickness
 *   3. Multi-material support
 *   4. Support structure generation
 *   5. Real-time preview rendering (OpenGL)
 * 
 * Dependency Additions:
 *   - CUDA Toolkit (for GPU acceleration)
 *   - OpenGL/Vulkan (for visualization)
 *   - Protobuf (for .marc format serialization)
 */

#pragma once

// This header is intentionally empty - it serves as documentation only
// Include this comment block in your project documentation

namespace MarcSLM {
namespace Config {

// Build configuration constants
constexpr const char* PROJECT_NAME = "MarcSLM";
constexpr const char* PROJECT_VERSION = "0.1.0";
constexpr const char* PROJECT_DESCRIPTION = "High-performance industrial DMLS/SLM slicer core";

// Performance tuning
constexpr size_t DEFAULT_SLICE_RESERVE_SIZE = 1000;
constexpr size_t PARALLEL_PROCESSING_THRESHOLD = 10; // Minimum layers for parallel processing

} // namespace Config
} // namespace MarcSLM
