# RBC App Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(rbc_app SHARED)

# Set EXPORT for compiling this target's own sources (PRIVATE)
target_compile_definitions(rbc_app PRIVATE RBC_APP_API=LUISA_DECLSPEC_DLL_EXPORT)
# Set IMPORT for other targets that link to this target (INTERFACE)
target_compile_definitions(rbc_app INTERFACE RBC_APP_API=LUISA_DECLSPEC_DLL_IMPORT)

# Apply basic settings
rbc_apply_basic_settings(rbc_app PROJECT_KIND shared)
rbc_set_precompiled_header(rbc_app ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(rbc_app PRIVATE ${SOURCES})

# Public interface: headers and dependencies that users of this library need
target_include_directories(rbc_app PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rbc_app PUBLIC
    rbc_runtime
    rbc_core
    rbc_render_plugin
)

# Private dependencies: only needed for building this library
# Link LuisaCompute targets - try both alias and actual names
if(TARGET lc-gui)
    target_link_libraries(rbc_app PRIVATE lc-gui)
elseif(TARGET luisa-compute-gui)
    target_link_libraries(rbc_app PRIVATE luisa-compute-gui)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(rbc_app PRIVATE luisa-compute-include)
endif()

