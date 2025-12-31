# RBC Render Plugin

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_shader_compile.cmake)

# Create shared library
add_library(rbc_render_plugin SHARED)

# Apply basic settings
rbc_apply_basic_settings(rbc_render_plugin PROJECT_KIND shared)
rbc_set_precompiled_header(rbc_render_plugin ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Add shader hostgen dependency
rbc_add_shader_hostgen(rbc_render_plugin)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(rbc_render_plugin PRIVATE ${SOURCES})

# Public interface: headers that users of this library need
target_include_directories(rbc_render_plugin PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shader/host
)

# Public dependencies: users of this library also need these (header-only)
# Note: oidn_plugin is header-only dependency, so we only need its headers
# In CMake, linking PUBLIC will propagate headers but not require linking the library
target_link_libraries(rbc_render_plugin PUBLIC
    oidn_plugin
)

# Private dependencies: only needed for building this library
# Link LuisaCompute targets - try both alias and actual names
if(TARGET lc-backends-dummy)
    target_link_libraries(rbc_render_plugin PRIVATE lc-backends-dummy)
elseif(TARGET luisa-compute-backends)
    target_link_libraries(rbc_render_plugin PRIVATE luisa-compute-backends)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(rbc_render_plugin PRIVATE luisa-compute-include)
endif()

target_link_libraries(rbc_render_plugin PRIVATE
    rbc_runtime
)

