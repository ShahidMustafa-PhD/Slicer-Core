# ==============================================================================
# MarcSLM - High-Performance Industrial DMLS/SLM Slicer Core
# ==============================================================================
# Copyright (c) 2024 MarcSLM Project
# Licensed under a commercial-friendly license (non-AGPL)
# ==============================================================================

cmake_minimum_required(VERSION 3.21)

# ==============================================================================
# Project Definition
# ==============================================================================
project(
    MarcSLM
    VERSION 0.1.0
    LANGUAGES CXX
    DESCRIPTION "High-performance C++ core geometry engine for industrial DMLS/SLM slicing"
)
# Force build type to Release
set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
message(STATUS "Build type forced to: ${CMAKE_BUILD_TYPE}")
# ==============================================================================
# C++ Standard Configuration
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Build Configuration Options
# ==============================================================================
option(MARCSLM_BUILD_TESTS "Build unit tests" OFF)
option(MARCSLM_BUILD_SHARED "Build shared library instead of static" OFF)

# ==============================================================================
# Output Directories
# ==============================================================================
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ==============================================================================
# CMake Module Path
# ==============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")

# ==============================================================================
# MSVC Compiler Configuration
# ==============================================================================
if(MSVC)
    # UTF-8 source and execution character sets
    add_compile_options(/utf-8)
    
    # Suppress C4828: illegal character in source file (non-UTF-8 bytes in comments/strings)
    add_compile_options(/wd4828)
    
    # Warning level 4 (high)
    add_compile_options(/W4)
    
    # Conformance mode: disable permissive behavior
    add_compile_options(/permissive-)
    
    # Disable CRT security warnings (for legacy API usage)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # Optimization flags for Release builds
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)
    
    # Link-time code generation for Release
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    
    # Generate debug info even in Release for crash analysis
    add_compile_options($<$<CONFIG:Release>:/Zi>)
    add_link_options($<$<CONFIG:Release>:/DEBUG>)
    add_link_options($<$<CONFIG:Release>:/OPT:REF>)
    add_link_options($<$<CONFIG:Release>:/OPT:ICF>)
    
    # Multi-processor compilation
    add_compile_options(/MP)
endif()

# ==============================================================================
# Dependency Resolution (vcpkg integration)
# ==============================================================================

find_package(assimp CONFIG REQUIRED)
find_package(Clipper2 CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Workaround for TBB debug library not found on Windows
# Use release TBB library and import library for debug builds if debug variants are not available
if(MSVC)
    get_target_property(_tbb_lib_debug TBB::tbb IMPORTED_LOCATION_DEBUG)
    if(NOT _tbb_lib_debug)
        get_target_property(_tbb_lib_release TBB::tbb IMPORTED_LOCATION_RELEASE)
        get_target_property(_tbb_implib_release TBB::tbb IMPORTED_IMPLIB_RELEASE)
        get_target_property(_tbb_implib_debug TBB::tbb IMPORTED_IMPLIB_DEBUG)
        
        if(_tbb_lib_release)
            # Set both LOCATION and IMPLIB for debug config
            set_target_properties(TBB::tbb PROPERTIES 
                IMPORTED_LOCATION_DEBUG "${_tbb_lib_release}"
            )
            
            # Only set IMPLIB_DEBUG if IMPLIB_RELEASE exists and IMPLIB_DEBUG doesn't
            if(_tbb_implib_release AND NOT _tbb_implib_debug)
                set_target_properties(TBB::tbb PROPERTIES 
                    IMPORTED_IMPLIB_DEBUG "${_tbb_implib_release}"
                )
            endif()
            
            if(CMAKE_BUILD_TYPE MATCHES "^(Debug|)$")
                message(STATUS "TBB: Using release library for debug builds (debug library not found)")
            endif()
        endif()
    endif()
endif()

# Try to find Eigen3; if not found, attempt to locate it in vcpkg installed path
find_package(Eigen3 CONFIG QUIET)
if(NOT TARGET Eigen3::Eigen)
    message(STATUS "Eigen3 not found by CMake find_package - attempting vcpkg paths...")
    # Determine vcpkg root and triplet from environment if available
    if(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_root $ENV{VCPKG_ROOT})
    elseif(DEFINED ENV{VCPKG_ROOT})
        set(_vcpkg_root $ENV{VCPKG_ROOT})
    else()
        set(_vcpkg_root "")
    endif()

    if(DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
        set(_vcpkg_triplet $ENV{VCPKG_DEFAULT_TRIPLET})
    elseif(DEFINED ENV{VCPKG_TARGET_TRIPLET})
        set(_vcpkg_triplet $ENV{VCPKG_TARGET_TRIPLET})
    else()
        # Default to x64-windows which is the common triplet for VS x64 builds
        set(_vcpkg_triplet "x64-windows")
    endif()

    if(_vcpkg_root STREQUAL "")
        message(WARNING "VCPKG_ROOT not set; if Eigen3 is installed via vcpkg, set VCPKG_ROOT environment variable to the vcpkg folder or configure CMake with -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake")
    else()
        set(_vcpkg_eigen_dir "${_vcpkg_root}/installed/${_vcpkg_triplet}/share/eigen3")
        if(EXISTS "${_vcpkg_eigen_dir}/Eigen3Config.cmake" OR EXISTS "${_vcpkg_eigen_dir}/eigen3-config.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_vcpkg_eigen_dir}")
            message(STATUS "Added vcpkg Eigen3 path to CMAKE_PREFIX_PATH: ${_vcpkg_eigen_dir}")
        else()
            message(WARNING "Eigen3Config.cmake not found in expected vcpkg path: ${_vcpkg_eigen_dir}")
            message(STATUS "You can also pass -DEigen3_DIR=<path-to-eigen3-share-dir> to CMake or use the vcpkg toolchain file")
        endif()
    endif()

    # Retry find
    find_package(Eigen3 CONFIG REQUIRED)
else()
    message(STATUS "Found Eigen3 (target Eigen3::Eigen)")
endif()

if(MARCSLM_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    enable_testing()
endif()

# ==============================================================================
# Source File Collection
# ==============================================================================
set(MARCSLM_CORE_HEADERS
    include/MarcSLM/Core/Types.hpp
    include/MarcSLM/Core/MarcFormat.hpp
    include/MarcSLM/Core/Config.hpp
    include/MarcSLM/Core/SlmConfig.hpp
    include/MarcSLM/Core/SlmConfigReader.hpp
    include/MarcSLM/Core/BuildStyleManager.hpp
    include/MarcSLM/Core/BuildPlate.hpp
    include/MarcSLM/Core/InternalModel.hpp
    include/MarcSLM/Core/MarcFile.hpp
    include/MarcSLM/Core/SVGExporter.hpp
    include/MarcSLM/Core/Logger.hpp
    include/MarcSLM/Core/ErrorState.hpp
    # BuildPlate sub-modules
    include/MarcSLM/Core/BuildPlate/BuildTypes.hpp
    include/MarcSLM/Core/BuildPlate/LayerHeightGenerator.hpp
    include/MarcSLM/Core/BuildPlate/SurfaceClassifier.hpp
    include/MarcSLM/Core/BuildPlate/OverhangDetector.hpp
    include/MarcSLM/Core/BuildPlate/SupportGenerator.hpp
    include/MarcSLM/Core/BuildPlate/LayerExporter.hpp
    include/MarcSLM/Core/BuildPlate/ModelPlacement.hpp
    include/MarcSLM/Core/BuildPlate/BuildPlatePreparator.hpp
)

set(MARCSLM_GEOMETRY_HEADERS
    include/MarcSLM/Geometry/MeshProcessor.hpp
    include/MarcSLM/Geometry/TriMesh.hpp
)

set(MARCSLM_GEOMETRY_SOURCES
    src/Geometry/MeshProcessor.cpp
    src/Geometry/TriMesh.cpp
)

set(MARCSLM_IO_HEADERS
    include/MarcSLM/IO/MeshData.hpp
    include/MarcSLM/IO/MeshIO.hpp
)

set(MARCSLM_IO_SOURCES
    src/IO/MeshIO.cpp
)

set(MARCSLM_THERMAL_HEADERS
    include/MarcSLM/Thermal/ThermalSegmentTypes.hpp
    include/MarcSLM/Thermal/ScanSegmentClassifier.hpp
    include/MarcSLM/Thermal/ThermalRegionHatcher.hpp
    include/MarcSLM/Thermal/LayerPolygonExtractor.hpp
    include/MarcSLM/Thermal/ClipperBoolean.hpp
)

set(MARCSLM_THERMAL_SOURCES
    src/Thermal/ScanSegmentClassifier.cpp
    src/Thermal/ThermalRegionHatcher.cpp
)

set(MARCSLM_PATHPLANNING_HEADERS
    include/MarcSLM/PathPlanning/HatchGenerator.hpp
    include/MarcSLM/PathPlanning/ScanVectorClipper.hpp
)

set(MARCSLM_PATHPLANNING_SOURCES
    src/PathPlanning/HatchGenerator.cpp
    src/PathPlanning/ScanVectorClipper.cpp
)

set(MARCSLM_ENGINE_HEADERS
    include/MarcSLM/SlmPrint.hpp
    include/MarcSLM/MarcAPI.hpp
)

set(MARCSLM_SOURCES
    # Core implementations
    src/Core/Logger.cpp
    src/Core/ErrorState.cpp
    src/Core/SlmConfigReader.cpp
    src/Core/BuildStyleManager.cpp
    src/Core/BuildPlate.cpp
    src/Core/MarcFile.cpp
    src/Core/SVGExporter.cpp
    # BuildPlate sub-module implementations
    src/Core/BuildPlate/BuildTypes.cpp
    src/Core/BuildPlate/LayerHeightGenerator.cpp
    src/Core/BuildPlate/SurfaceClassifier.cpp
    src/Core/BuildPlate/OverhangDetector.cpp
    src/Core/BuildPlate/SupportGenerator.cpp
    src/Core/BuildPlate/LayerExporter.cpp
    src/Core/BuildPlate/ModelPlacement.cpp
    src/Core/BuildPlate/BuildPlatePreparator.cpp
    
    # Geometry implementations
    ${MARCSLM_GEOMETRY_SOURCES}
    
    # I/O implementations
    ${MARCSLM_IO_SOURCES}
    
    # Thermal implementations
    ${MARCSLM_THERMAL_SOURCES}
    
    # PathPlanning implementations
    ${MARCSLM_PATHPLANNING_SOURCES}

    # Engine implementations
    src/SlmPrint.cpp
    src/MarcAPI.cpp
)

# Combine all headers
set(MARCSLM_ALL_HEADERS
    ${MARCSLM_CORE_HEADERS}
    ${MARCSLM_GEOMETRY_HEADERS}
    ${MARCSLM_IO_HEADERS}
    ${MARCSLM_THERMAL_HEADERS}
    ${MARCSLM_PATHPLANNING_HEADERS}
    ${MARCSLM_ENGINE_HEADERS}
)

# ==============================================================================
# Target Definition: MarcSLM_Core Library
# ==============================================================================
if(MARCSLM_BUILD_SHARED)
    add_library(MarcSLM_Core SHARED ${MARCSLM_SOURCES} ${MARCSLM_ALL_HEADERS})
    target_compile_definitions(MarcSLM_Core PRIVATE MARCSLM_EXPORTS)
    target_compile_definitions(MarcSLM_Core INTERFACE MARCSLM_IMPORTS)
else()
    add_library(MarcSLM_Core STATIC ${MARCSLM_SOURCES} ${MARCSLM_ALL_HEADERS})
endif()

# Create alias for consistent naming
add_library(MarcSLM::Core ALIAS MarcSLM_Core)

# ==============================================================================
# Target Definition: MarcSLM_CLI Executable (Command-Line Interface)
# ==============================================================================
add_executable(MarcSLM_CLI src/main.cpp)
target_link_libraries(MarcSLM_CLI PRIVATE MarcSLM::Core)
target_compile_features(MarcSLM_CLI PRIVATE cxx_std_17)

# Set the output name to MarcSLM
set_target_properties(MarcSLM_CLI PROPERTIES
    OUTPUT_NAME "MarcSLM"
)

# ==============================================================================
# Target Definition: io_example Executable (Mesh I/O Test Tool)
# ==============================================================================
add_executable(io_example src/io_example.cpp)
target_link_libraries(io_example PRIVATE MarcSLM::Core)
target_compile_features(io_example PRIVATE cxx_std_17)

# ==============================================================================
# Post-build step: Copy MarcSLM_CLI and dependencies to bin and install/bin after build
# ==============================================================================
if(MSVC)
    #set(POST_BUILD_INSTALL_DIR "${CMAKE_BINARY_DIR}/install/bin")
    set(POST_BUILD_INSTALL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/install")
    set(POST_BUILD_BIN_DIR "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    add_custom_command(TARGET MarcSLM_CLI POST_BUILD
        # Copy all DLLs to bin directory (runtime output dir)
        COMMAND ${CMAKE_COMMAND} -Dsrc_dir="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" -Ddst_dir="${POST_BUILD_BIN_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/CopyDlls.cmake"
        # Copy all DLLs to install/bin directory
        COMMAND ${CMAKE_COMMAND} -Dsrc_dir="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}" -Ddst_dir="${POST_BUILD_INSTALL_DIR}" -P "${CMAKE_SOURCE_DIR}/cmake/CopyDlls.cmake"
        # Copy the main executable to install/bin
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:MarcSLM_CLI> "${POST_BUILD_INSTALL_DIR}"
        COMMENT "Copying MarcSLM_CLI and all DLL dependencies to bin and install/bin after build"
    )
endif()

# ==============================================================================
# Include Directories
# ==============================================================================
target_include_directories(MarcSLM_Core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(MarcSLM_CLI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_include_directories(io_example
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ==============================================================================
# Link Dependencies
# ==============================================================================
target_link_libraries(MarcSLM_Core
    PUBLIC
         
   
        # Math primitives
        glm::glm
        
        # Linear algebra
        Eigen3::Eigen
        
        # 2D topology engine
        Clipper2::Clipper2
        
    PRIVATE
        # Model I/O
        assimp::assimp
        
        # Parallel processing
        TBB::tbb
)

# ==============================================================================
# Compiler Feature Requirements
# ==============================================================================
target_compile_features(MarcSLM_Core PUBLIC cxx_std_17)

# ==============================================================================
# IDE Support: Source Grouping
# ==============================================================================
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/include" 
             PREFIX "Header Files" 
             FILES ${MARCSLM_ALL_HEADERS})

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" 
             PREFIX "Source Files" 
             FILES ${MARCSLM_SOURCES})

# ==============================================================================
# Unit Tests
# ==============================================================================
if(MARCSLM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# ==============================================================================
# Installation Rules
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS MarcSLM_Core
    EXPORT MarcSLM_CoreTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/MarcSLM
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT MarcSLM_CoreTargets
    FILE MarcSLM_CoreTargets.cmake
    NAMESPACE MarcSLM::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MarcSLM
)

# ==============================================================================
# Package Configuration
# ==============================================================================
include(CMakePackageConfigHelpers)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MarcSLM_CoreConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/MarcSLM_CoreConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MarcSLM
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/MarcSLM_CoreConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/MarcSLM_CoreConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/MarcSLM_CoreConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/MarcSLM
)
# ==============================================================================
# Summary
# ==============================================================================
message(STATUS "========================================")
message(STATUS "MarcSLM Configuration Summary")
message(STATUS "========================================")
message(STATUS "Version:         ${PROJECT_VERSION}")
message(STATUS "Build Type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "Library Type:    ${MARCSLM_BUILD_SHARED}")
message(STATUS "Build Tests:     ${MARCSLM_BUILD_TESTS}")
message(STATUS "Install Prefix:  ${CMAKE_INSTALL_PREFIX}")
message(STATUS "========================================")
