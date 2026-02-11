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
    
    # Geometry tests (to be added)
    # test_geometry.cpp
    
    # Thermal classification tests (to be added)
    # test_thermal.cpp
    
    # Path planning tests (to be added)
    # test_pathplanning.cpp
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
