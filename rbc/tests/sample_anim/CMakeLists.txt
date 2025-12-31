# Sample Animation

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(sample_anim ${TEST_SOURCES})

rbc_apply_basic_settings(sample_anim PROJECT_KIND binary)

target_link_libraries(sample_anim PRIVATE
    rbc_app
    rbc_render_plugin
    rbc_runtime
    rbc_core
)

# Link LuisaCompute GUI for Window support
if(TARGET lc-gui)
    target_link_libraries(sample_anim PRIVATE lc-gui)
elseif(TARGET luisa-compute-gui)
    target_link_libraries(sample_anim PRIVATE luisa-compute-gui)
endif()

if(TARGET luisa-compute-include)
    target_link_libraries(sample_anim PRIVATE luisa-compute-include)
endif()

