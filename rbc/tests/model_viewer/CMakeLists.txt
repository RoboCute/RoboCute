# Model Viewer

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

add_executable(model_viewer
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

rbc_apply_basic_settings(model_viewer PROJECT_KIND binary)

target_link_libraries(model_viewer PRIVATE
    rbc_runtime
    rbc_core
)

