# RBC Core Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(rbc_core SHARED)

# Set EXPORT for compiling this target's own sources (PRIVATE)
target_compile_definitions(rbc_core PRIVATE RBC_CORE_API=LUISA_DECLSPEC_DLL_EXPORT)
# Set IMPORT for other targets that link to this target (INTERFACE)
target_compile_definitions(rbc_core INTERFACE RBC_CORE_API=LUISA_DECLSPEC_DLL_IMPORT)

# Apply basic settings
rbc_apply_basic_settings(rbc_core PROJECT_KIND shared)
rbc_enable_unity_build(rbc_core 4)
rbc_set_precompiled_header(rbc_core ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# mimalloc option (matches rbc_config.h)
option(RBC_RUNTIME_USE_MIMALLOC "Use mimalloc for memory management" ON)

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
# LMDB is needed because luisa-compute-vstl uses it (OBJECT library PRIVATE deps don't propagate)
if(TARGET luisa-compute-ext-lmdb)
    target_link_libraries(rbc_core PRIVATE luisa-compute-ext-lmdb)
endif()

# Always link luisa-compute-include for headers
if(TARGET luisa-compute-include)
    target_link_libraries(rbc_core PRIVATE luisa-compute-include)
endif()

# Add mimalloc include path if RBC_RUNTIME_USE_MIMALLOC is enabled
if(RBC_RUNTIME_USE_MIMALLOC)
    set(MIMALLOC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/LuisaCompute/src/ext/EASTL/packages/mimalloc/include")
    if(EXISTS "${MIMALLOC_INCLUDE_DIR}/mimalloc.h")
        target_include_directories(rbc_core PRIVATE "${MIMALLOC_INCLUDE_DIR}")
    else()
        # Try another possible path
        set(MIMALLOC_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/thirdparty/luisa_compute/src/ext/EASTL/packages/mimalloc/include")
        if(EXISTS "${MIMALLOC_INCLUDE_DIR}/mimalloc.h")
            target_include_directories(rbc_core PRIVATE "${MIMALLOC_INCLUDE_DIR}")
        endif()
    endif()
    
    # Link to mimalloc-static if it exists
    if(TARGET mimalloc-static)
        target_link_libraries(rbc_core PRIVATE mimalloc-static)
    endif()
endif()

target_link_libraries(rbc_core PRIVATE rtm)

