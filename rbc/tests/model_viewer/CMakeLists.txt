# Model Viewer

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(model_viewer ${TEST_SOURCES})

rbc_apply_basic_settings(model_viewer PROJECT_KIND binary)

target_link_libraries(model_viewer PRIVATE
    rbc_runtime
    rbc_core
)

