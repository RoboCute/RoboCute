# RBC Node Graph Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(rbc_node SHARED)

# Set EXPORT for compiling this target's own sources (PRIVATE)
target_compile_definitions(rbc_node PRIVATE RBC_NODE_API=LUISA_DECLSPEC_DLL_EXPORT)
# Set IMPORT for other targets that link to this target (INTERFACE)
target_compile_definitions(rbc_node INTERFACE RBC_NODE_API=LUISA_DECLSPEC_DLL_IMPORT)

# Apply basic settings
rbc_apply_basic_settings(rbc_node PROJECT_KIND shared)
rbc_set_precompiled_header(rbc_node ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(rbc_node PRIVATE ${SOURCES})

# Public interface: headers that users of this library need
target_include_directories(rbc_node PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Public dependencies: users of this library also need these
target_link_libraries(rbc_node PUBLIC
    rbc_runtime
)

# Link LuisaCompute core for DSL support
if(TARGET lc-core)
    target_link_libraries(rbc_node PUBLIC lc-core)
elseif(TARGET luisa-compute-core)
    target_link_libraries(rbc_node PUBLIC luisa-compute-core)
endif()

# Private dependencies: only needed for building this library
if(TARGET luisa-compute-include)
    target_link_libraries(rbc_node PRIVATE luisa-compute-include)
endif()

