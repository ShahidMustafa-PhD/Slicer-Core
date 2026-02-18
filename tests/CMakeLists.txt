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
    
    # MarcFile binary I/O tests
    test_marc_file_io.cpp
    
    # Hatch generator tests
    test_hatch_generator.cpp
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

    # Enable CTest integration
    include(GoogleTest)
    gtest_discover_tests(MarcSLM_Tests)
else()
    message(STATUS "No test sources found - skipping test executable creation")
endif()
