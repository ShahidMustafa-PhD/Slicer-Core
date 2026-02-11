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

# ==============================================================================
# C++ Standard Configuration
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Build Configuration Options
# ==============================================================================
option(MARCSLM_BUILD_TESTS "Build unit tests" ON)
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

find_package(manifold CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Clipper2 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

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
)

set(MARCSLM_GEOMETRY_HEADERS
    include/MarcSLM/Geometry/MeshProcessor.hpp
)

set(MARCSLM_GEOMETRY_SOURCES
    src/Geometry/MeshProcessor.cpp
)

set(MARCSLM_THERMAL_HEADERS
    # Will be populated as thermal classification components are added
)

set(MARCSLM_PATHPLANNING_HEADERS
    # Will be populated as path planning components are added
)

set(MARCSLM_SOURCES
    # Core implementations
    # src/Core/...
    
    # Geometry implementations
    ${MARCSLM_GEOMETRY_SOURCES}
    
    # Thermal implementations
    # src/Thermal/...
    
    # PathPlanning implementations
    # src/PathPlanning/...
)

# Combine all headers
set(MARCSLM_ALL_HEADERS
    ${MARCSLM_CORE_HEADERS}
    ${MARCSLM_GEOMETRY_HEADERS}
    ${MARCSLM_THERMAL_HEADERS}
    ${MARCSLM_PATHPLANNING_HEADERS}
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
# Include Directories
# ==============================================================================
target_include_directories(MarcSLM_Core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
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
        
        # 2D topology engine
        Clipper2::Clipper2
        
        # 3D manifold processing
        manifold::manifold
        
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
