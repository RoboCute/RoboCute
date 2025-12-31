# RBC Runtime Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(rbc_runtime SHARED)

# Set EXPORT for compiling this target's own sources (PRIVATE)
target_compile_definitions(rbc_runtime PRIVATE RBC_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT)
# Set IMPORT for other targets that link to this target (INTERFACE)
target_compile_definitions(rbc_runtime INTERFACE RBC_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT)

# Apply basic settings
rbc_apply_basic_settings(rbc_runtime PROJECT_KIND shared)
rbc_enable_unity_build(rbc_runtime 8)
rbc_set_precompiled_header(rbc_runtime ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(rbc_runtime PRIVATE ${SOURCES})

# Fix unity build conflict for duplicate filenames
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/anim/asset/anim_sequence.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/world/resources/anim_sequence.cpp
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE
)

# Public interface: headers and dependencies that users of this library need
target_include_directories(rbc_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shader/include
)

target_link_libraries(rbc_runtime PUBLIC
    lc-runtime
    rbc_core
    ozz_animation_include
)

# Private dependencies: only needed for building this library
# Link LuisaCompute targets - try both alias and actual names
if(TARGET lc-volk)
    target_link_libraries(rbc_runtime PRIVATE lc-volk)
elseif(TARGET luisa-compute-ext-volk)
    target_link_libraries(rbc_runtime PRIVATE luisa-compute-ext-volk)
endif()

if(TARGET lc-runtime)
    target_link_libraries(rbc_runtime PRIVATE lc-runtime)
elseif(TARGET luisa-compute-runtime)
    target_link_libraries(rbc_runtime PRIVATE luisa-compute-runtime)
endif()

if(TARGET lc-backends-dummy)
    target_link_libraries(rbc_runtime PRIVATE lc-backends-dummy)
elseif(TARGET luisa-compute-backends)
    target_link_libraries(rbc_runtime PRIVATE luisa-compute-backends)
endif()

# LMDB is needed because luisa-compute-runtime uses it (OBJECT library PRIVATE deps don't propagate)
if(TARGET luisa-compute-ext-lmdb)
    target_link_libraries(rbc_runtime PRIVATE luisa-compute-ext-lmdb)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(rbc_runtime PRIVATE luisa-compute-include)
endif()

target_link_libraries(rbc_runtime PRIVATE
    tinyexr
    tiny_obj_loader
    open_fbx
    tinytiff
    tinygltf
    ozz_animation_runtime_static
    ozz_animation_offline_static
)

# Note: stb-image is header-only, provided by LuisaCompute

# Windows-specific system libraries
if(WIN32)
    target_link_libraries(rbc_runtime PRIVATE d3d12 User32)
endif()

