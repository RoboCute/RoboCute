# Test Serde

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_serde ${TEST_SOURCES})

rbc_apply_basic_settings(test_serde PROJECT_KIND binary)

target_link_libraries(test_serde PRIVATE
    rbc_core
)

