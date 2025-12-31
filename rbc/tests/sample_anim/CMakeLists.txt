# Sample Animation

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

add_executable(sample_anim
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

rbc_apply_basic_settings(sample_anim PROJECT_KIND binary)

target_link_libraries(sample_anim PRIVATE
    rbc_runtime
    rbc_core
)

