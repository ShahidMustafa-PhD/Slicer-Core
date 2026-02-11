cmake_minimum_required(VERSION 3.9)
project(mardll LANGUAGES C CXX)

# ============================================================================
# Build Configuration
# ============================================================================
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================================
# Options
# ============================================================================
option(Enable_GUI "Use the wxWidgets code in slic3r.cpp" OFF)
option(GUI_BUILD_TESTS "Build tests for Slic3r GUI." OFF)
option(SLIC3R_BUILD_TESTS "Build tests for libslic3r." OFF)
option(SLIC3R_STATIC "Build and link Slic3r statically." ON)
option(BUILD_EXTRUDE_TIN "Build and link the extrude-tin application." OFF)
option(PROFILE "Build with gprof profiling output." OFF)
option(COVERAGE "Build with gcov code coverage profiling." OFF)
option(SLIC3R_DEBUG "Build with Slic3r's debug output" OFF)
option(SLIC3R_LIBRARY "Build Slic3r's Library Only" OFF)
option(MARC_LIBRARY "Build Marc Library Only" ON)
option(MARC_API "Build Marc DLL Only" ON)

# ============================================================================
# Compiler Flags
# ============================================================================
if(MINGW)
    add_compile_options(-Wa,-mbig-obj)
endif()

add_compile_options(-ftemplate-backtrace-limit=0)
add_compile_options(-DNO_PERL -DM_PI=3.14159265358979323846 -DHAS_BOOL -DNOGDI -DBOOST_ASIO_DISABLE_KQUEUE)

if(SLIC3R_DEBUG)
    add_compile_options(-DSLIC3R_DEBUG)
endif()

if(MSVC)
    add_compile_options(-W3 -bigobj)
else()
    add_compile_options(-Wall)
endif()

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_compile_options(-g -O0)
elseif(CMAKE_BUILD_TYPE MATCHES Release)
    add_compile_options(-O3)
elseif(CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
    add_compile_options(-g -O2)
endif()

if(PROFILE)
    add_compile_options(-g -pg -DBUILD_PROFILE)
endif()

if(COVERAGE)
    add_compile_options(-g -ftest-coverage)
endif()

if(Enable_GUI)
    add_compile_options(-DUSE_WX)
endif()

# ============================================================================
# External Dependencies - CRITICAL: Set BEFORE find_package
# ============================================================================

# NLopt (optional)
set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
#find_path(NLOPT_INCLUDE_DIR nlopt.hpp PATHS "${CMAKE_PREFIX_PATH}/include")
#ind_library(NLOPT_LIBRARY nlopt PATHS "${CMAKE_PREFIX_PATH}/lib")
#message(STATUS "NLopt include dir: ${NLOPT_INCLUDE_DIR}")
#message(STATUS "NLopt library: ${NLOPT_LIBRARY}")

# ============================================================================
# Boost - SET VARIABLES BEFORE find_package
# ============================================================================
set(BOOST_ROOT "C:/Dev/Boost-mingw-static")
set(BOOST_INCLUDEDIR "C:/Dev/Boost-mingw-static/include")
set(BOOST_LIBRARYDIR "C:/Dev/Boost-mingw-static/lib")

# Set policies BEFORE find_package
if(POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW)
endif()
if(POLICY CMP0144)
  cmake_policy(SET CMP0144 NEW)
endif()
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# Set Boost static/dynamic BEFORE find_package
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# Force FindBoost module
set(Boost_NO_BOOST_CMAKE ON)

# Try find_package first
find_package(Boost QUIET COMPONENTS system filesystem)

# If not found, manually set the libraries (CRITICAL FIX)
if(NOT Boost_FOUND)
    message(STATUS "Boost libraries not found via find_package, using manual paths")
    
    set(Boost_FOUND TRUE)
    set(Boost_INCLUDE_DIRS "C:/Dev/Boost-mingw-static/include")
    
    # Manually set library paths matching your Boost build
    set(Boost_SYSTEM_LIBRARY "C:/Dev/Boost-mingw-static/lib/libboost_system-gcc15-mt-x64-1_86.a")
    set(Boost_FILESYSTEM_LIBRARY "C:/Dev/Boost-mingw-static/lib/libboost_filesystem-gcc15-mt-x64-1_86.a")
    
    set(Boost_LIBRARIES
        ${Boost_SYSTEM_LIBRARY}
        ${Boost_FILESYSTEM_LIBRARY}
    )
    
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
else()
    message(STATUS "Boost found via find_package")
    message(STATUS "Boost include: ${Boost_INCLUDE_DIRS}")
    message(STATUS "Boost libraries: ${Boost_LIBRARIES}")
endif()

# Optional: Check for nowide
if(NOT (${Boost_VERSION_STRING} VERSION_LESS "1.74.0"))
    message(STATUS "Boost version >= 1.74.0: Nowide support available")
    set(BOOST_NOWIDE_FOUND ON)
endif()

find_package(Threads REQUIRED)
# ============================================================================
# Include Directories
# ============================================================================
set(LIBDIR ${CMAKE_CURRENT_SOURCE_DIR}/src/)

set(EXPAT_INCLUDES ${LIBDIR}/expat)
set(ADMESH_INCLUDES ${LIBDIR}/admesh)
set(BSPLINE_INCLUDES ${LIBDIR}/BSpline/)
set(POLY2TRI_INCLUDES ${LIBDIR}/poly2tri ${LIBDIR}/poly2tri/sweep ${LIBDIR}/poly2tri/common)
set(COMMON_INCLUDES ${LIBDIR} ${CMAKE_CURRENT_SOURCE_DIR}/standalone/)

set(SLIC3R_INCLUDES
    ${COMMON_INCLUDES}
    ${EXPAT_INCLUDES}
    ${ADMESH_INCLUDES}
    ${POLY2TRI_INCLUDES}
    ${BSPLINE_INCLUDES}
)

set(LIBSLIC3R_INCLUDES
    ${LIBDIR}/libslic3r
    ${LIBDIR}/libslic3r/IO
    #${NLOPT_INCLUDE_DIR}
)

set(LIBMARC_INCLUDES
    ${LIBDIR}/libslic3r
    ${LIBDIR}/libslic3r/IO
    ${LIBDIR}/libslic3r/Fill
    ${LIBDIR}/libslic3r/GCode
    ${LIBDIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/marc_src
    ${CMAKE_CURRENT_SOURCE_DIR}/marcAPI_manager
    ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/include
)

# ============================================================================
# Third-party Libraries
# ============================================================================
add_library(ZipArchive STATIC ${LIBDIR}/Zip/ZipArchive.cpp)
target_compile_features(ZipArchive PUBLIC cxx_std_11)
target_include_directories(ZipArchive PUBLIC ${COMMON_INCLUDES})
target_compile_options(ZipArchive PUBLIC -w)
set_target_properties(ZipArchive PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(miniz STATIC ${LIBDIR}/miniz/miniz.c)
target_compile_options(miniz PUBLIC -w)

add_library(BSpline STATIC ${LIBDIR}/BSpline/BSpline.cpp)
target_include_directories(BSpline PUBLIC ${BSPLINE_INCLUDES})
target_compile_options(BSpline PUBLIC -w)

add_library(admesh STATIC
    ${LIBDIR}/admesh/connect.c
    ${LIBDIR}/admesh/normals.c
    ${LIBDIR}/admesh/shared.c
    ${LIBDIR}/admesh/stl_io.c
    ${LIBDIR}/admesh/stlinit.c
    ${LIBDIR}/admesh/util.c
)
target_include_directories(admesh PUBLIC ${ADMESH_INCLUDES} ${COMMON_INCLUDES})
set_property(TARGET admesh PROPERTY C_STANDARD 99)
target_compile_options(admesh PUBLIC -w)
set_target_properties(admesh PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(clipper STATIC ${LIBDIR}/clipper.cpp)
target_compile_features(clipper PUBLIC cxx_std_11)
target_include_directories(clipper PUBLIC ${COMMON_INCLUDES})
target_compile_options(clipper PUBLIC -w)

add_library(expat STATIC
    ${LIBDIR}/expat/xmlparse.c
    ${LIBDIR}/expat/xmlrole.c
    ${LIBDIR}/expat/xmltok.c
)
target_include_directories(expat PUBLIC ${EXPAT_INCLUDES})
target_compile_options(expat PUBLIC -w)

add_library(polypartition STATIC ${LIBDIR}/polypartition.cpp)
target_include_directories(polypartition PUBLIC ${COMMON_INCLUDES})
target_compile_options(polypartition PUBLIC -w)

add_library(poly2tri STATIC
    ${LIBDIR}/poly2tri/common/shapes.cc
    ${LIBDIR}/poly2tri/sweep/advancing_front.cc
    ${LIBDIR}/poly2tri/sweep/cdt.cc
    ${LIBDIR}/poly2tri/sweep/sweep_context.cc
    ${LIBDIR}/poly2tri/sweep/sweep.cc
)
target_include_directories(poly2tri PUBLIC ${COMMON_INCLUDES})
target_compile_options(poly2tri PUBLIC -w)
set_target_properties(poly2tri PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ============================================================================
# Main Slic3r Library
# ============================================================================
add_library(slic3r STATIC
    ${LIBDIR}/libslic3r/BoundingBox.cpp
    ${LIBDIR}/libslic3r/BridgeDetector.cpp
    ${LIBDIR}/libslic3r/ClipperUtils.cpp
    ${LIBDIR}/libslic3r/ExPolygon.cpp
    ${LIBDIR}/libslic3r/ExPolygonCollection.cpp
    ${LIBDIR}/libslic3r/Extruder.cpp
    ${LIBDIR}/libslic3r/ExtrusionEntity.cpp
    ${LIBDIR}/libslic3r/ExtrusionEntityCollection.cpp
    ${LIBDIR}/libslic3r/Fill/Fill.cpp
    ${LIBDIR}/libslic3r/Fill/FillSLMisland.cpp
    ${LIBDIR}/libslic3r/Flow.cpp
    ${LIBDIR}/libslic3r/Geometry.cpp
    ${LIBDIR}/libslic3r/IO.cpp
    ${LIBDIR}/libslic3r/Layer.cpp
    ${LIBDIR}/libslic3r/LayerRegion.cpp
    ${LIBDIR}/libslic3r/LayerRegionFill.cpp
    ${LIBDIR}/libslic3r/LayerHeightSpline.cpp
    ${LIBDIR}/libslic3r/Line.cpp
    ${LIBDIR}/libslic3r/Log.cpp
    ${LIBDIR}/libslic3r/Model.cpp
    ${LIBDIR}/libslic3r/MultiPoint.cpp
    ${LIBDIR}/libslic3r/PerimeterGenerator.cpp
    ${LIBDIR}/libslic3r/PlaceholderParser.cpp
    ${LIBDIR}/libslic3r/Point.cpp
    ${LIBDIR}/libslic3r/Polygon.cpp
    ${LIBDIR}/libslic3r/Polyline.cpp
    ${LIBDIR}/libslic3r/PolylineCollection.cpp
    ${LIBDIR}/libslic3r/Print.cpp
    ${LIBDIR}/libslic3r/PrintObject.cpp
    ${LIBDIR}/libslic3r/PrintRegion.cpp
    ${LIBDIR}/libslic3r/SimplePrint.cpp
    ${LIBDIR}/libslic3r/SlicingAdaptive.cpp
    ${LIBDIR}/libslic3r/Surface.cpp
    ${LIBDIR}/libslic3r/SurfaceCollection.cpp
    ${LIBDIR}/libslic3r/SVG.cpp
    ${LIBDIR}/libslic3r/TriangleMesh.cpp
    ${LIBDIR}/libslic3r/TransformationMatrix.cpp
    ${LIBDIR}/libslic3r/utils.cpp
    ${LIBDIR}/libslic3r/miniz_extension.cpp
    ${LIBDIR}/libslic3r/MeshOrientationOptimizer.cpp
    ${LIBDIR}/libslic3r/SlmHatch.cpp
)

target_compile_features(slic3r PUBLIC cxx_std_17)
target_include_directories(slic3r SYSTEM PUBLIC ${SLIC3R_INCLUDES})
target_include_directories(slic3r PUBLIC ${LIBSLIC3R_INCLUDES})
target_include_directories(slic3r SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})  # ADD THIS LINE
set_target_properties(slic3r PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(BOOST_NOWIDE_FOUND)
    target_compile_options(slic3r PRIVATE -DBOOST_INCLUDE_NOWIDE)
endif()

# Link Slic3r dependencies (conditional NLopt)
target_link_libraries(slic3r PRIVATE
    admesh
    BSpline
    clipper
    expat
    polypartition
    poly2tri
    ZipArchive
    miniz
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

#if(NLOPT_LIBRARY)
    #target_link_libraries(slic3r PRIVATE ${NLOPT_LIBRARY})
#endif()

# ============================================================================
# Marc Library (Static)
# ============================================================================
if(MARC_LIBRARY)
    set(MARC_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/MarcFile.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/ExportMarcSlice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/SVG.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/marcToSVG.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/SlmPrint.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/MarcAPI.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/Logger.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/ErrorState.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/slmHatches.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marc_src/scanSegments.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/marcAPI_manager/MarcAPIManager.cpp
    )

    add_library(marc STATIC ${MARC_SRC})
    target_include_directories(marc PRIVATE ${LIBMARC_INCLUDES} ${Boost_INCLUDE_DIRS})
    set_target_properties(marc PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        POSITION_INDEPENDENT_CODE ON
    )
    target_link_libraries(marc PRIVATE slic3r ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

# ============================================================================
# Test Executable
# ============================================================================
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Test)
add_executable(Marctest ${CMAKE_CURRENT_SOURCE_DIR}/marcAPI_manager/Main.cpp)
target_include_directories(Marctest PRIVATE ${LIBMARC_INCLUDES})
target_link_libraries(Marctest PRIVATE marc ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(Marctest PROPERTIES CXX_STANDARD 17)

# ============================================================================
# Marc DLL API
# ============================================================================
if(MARC_API)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build)
    set(LIBDLL ${CMAKE_CURRENT_SOURCE_DIR}/marc_dll)

    add_library(marcapi SHARED
        ${LIBDLL}/MarcAPIInterface.cpp
        ${LIBDLL}/MarcAPIInterface.h
    )

    target_compile_definitions(marcapi PRIVATE MARC_EXPORTS)
    target_include_directories(marcapi PRIVATE ${LIBMARC_INCLUDES} ${Boost_INCLUDE_DIRS})
    set_target_properties(marcapi PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
    target_link_libraries(marcapi PRIVATE marc ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

# ============================================================================
# Summary
# ============================================================================
message(STATUS "========================================")
message(STATUS "Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Slic3r Static: ${SLIC3R_STATIC}")
message(STATUS "  Marc Library: ${MARC_LIBRARY}")
message(STATUS "  Marc API (DLL): ${MARC_API}")
message(STATUS "  Boost Version: ${Boost_VERSION_STRING}")
message(STATUS "========================================")