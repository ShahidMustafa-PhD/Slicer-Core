# ==============================================================================
# MarcSLM - Unit Tests
# ==============================================================================

# ==============================================================================
# Test Executable Definition
# ==============================================================================
set(TEST_SOURCES
    # Core type tests
    test_types.cpp
    test_marc_format.cpp
    test_mesh_processor.cpp
    
    # I/O tests
    test_mesh_io.cpp
    
    # SLM Config tests
    test_slm_config.cpp
    
    # Build Style tests
    test_build_style.cpp
    
    # Thermal classification tests
    test_thermal_segments.cpp
    test_scan_segment_classifier.cpp
    test_thermal_region_hatcher.cpp
    
    # MarcFile binary I/O tests
    test_marc_file_io.cpp
    
    # Hatch generator tests
    test_hatch_generator.cpp
    
    # Build plate tests
    test_build_plate.cpp
    
    # Build plate preparation tests
    test_build_plate_preparation.cpp
)

# Only create test executable if we have test sources
if(TEST_SOURCES)
    add_executable(MarcSLM_Tests ${TEST_SOURCES})

    target_link_libraries(MarcSLM_Tests
        PRIVATE
            MarcSLM::Core
            GTest::gtest
            GTest::gtest_main
    )

    # Post-build: Copy vcpkg DLLs to test output directory (Windows only)
    # This MUST happen before gtest_discover_tests runs the exe
    if(WIN32)
        set(VCPKG_DLLS
            "$ENV{VCPKG_ROOT}/installed/x64-windows/bin/gtest.dll"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/bin/gtest_main.dll"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/bin/tbb12.dll"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/bin/assimp-vc143-mt.dll"
            # Add more DLLs here if needed
        )
        foreach(dll ${VCPKG_DLLS})
            if(EXISTS ${dll})
                add_custom_command(TARGET MarcSLM_Tests POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${dll}"
                        $<TARGET_FILE_DIR:MarcSLM_Tests>
                )
            endif()
        endforeach()
    endif()

    # Enable CTest integration
    # Use PRE_TEST discovery so the exe is not run during build time
    # (avoids DLL-not-found errors during build)
    include(GoogleTest)
    gtest_discover_tests(MarcSLM_Tests DISCOVERY_MODE PRE_TEST)
else()
    message(STATUS "No test sources found - skipping test executable creation")
endif()

