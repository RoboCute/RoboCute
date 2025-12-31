# RBC Core Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(rbc_core SHARED)

# Set EXPORT for compiling this target's own sources (PRIVATE)
target_compile_definitions(rbc_core PRIVATE RBC_CORE_API=LUISA_DECLSPEC_DLL_EXPORT)
# Set IMPORT for other targets that link to this target (PUBLIC)
target_compile_definitions(rbc_core PUBLIC RBC_CORE_API=LUISA_DECLSPEC_DLL_IMPORT)

# Apply basic settings
rbc_apply_basic_settings(rbc_core PROJECT_KIND shared)
rbc_enable_unity_build(rbc_core 4)
rbc_set_precompiled_header(rbc_core ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(rbc_core PRIVATE ${SOURCES})

# Public interface: headers and dependencies that users of this library need
target_include_directories(rbc_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rbc_core PUBLIC
    lc-core
    lc-vstl
    lc-yyjson
    RBCTracy
)

# Private dependencies: only needed for building this library
# Link LuisaCompute targets - try both alias names and actual target names
if(TARGET lc-core)
    target_link_libraries(rbc_core PRIVATE lc-core)
elseif(TARGET luisa-compute-core)
    target_link_libraries(rbc_core PRIVATE luisa-compute-core)
endif()

if(TARGET lc-vstl)
    target_link_libraries(rbc_core PRIVATE lc-vstl)
elseif(TARGET luisa-compute-vstl)
    target_link_libraries(rbc_core PRIVATE luisa-compute-vstl)
endif()

# LMDB is needed because luisa-compute-vstl uses it (OBJECT library PRIVATE deps don't propagate)
if(TARGET luisa-compute-ext-lmdb)
    target_link_libraries(rbc_core PRIVATE luisa-compute-ext-lmdb)
endif()

if(TARGET lc-yyjson)
    target_link_libraries(rbc_core PRIVATE lc-yyjson)
elseif(TARGET luisa-compute-ext-yyjson)
    target_link_libraries(rbc_core PRIVATE luisa-compute-ext-yyjson)
elseif(TARGET luisa-compute-ext-yyjson-interface)
    target_link_libraries(rbc_core PRIVATE luisa-compute-ext-yyjson-interface)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(rbc_core PRIVATE luisa-compute-include)
endif()

# Add mimalloc include path if RBC_RUNTIME_USE_MIMALLOC is enabled
if(RBC_RUNTIME_USE_MIMALLOC)
    set(MIMALLOC_DIR "${CMAKE_SOURCE_DIR}/thirdparty/LuisaCompute/src/ext/EASTL/packages/mimalloc")
    if(EXISTS "${MIMALLOC_DIR}/include/mimalloc.h")
        target_include_directories(rbc_core PRIVATE "${MIMALLOC_DIR}/include")
    elseif(EXISTS "${MIMALLOC_DIR}/mimalloc.h")
        target_include_directories(rbc_core PRIVATE "${MIMALLOC_DIR}")
    endif()
endif()

target_link_libraries(rbc_core PRIVATE
    rtm
    RBCTracy
)

