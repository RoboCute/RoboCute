# RBC IPC Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Create shared library
add_library(rbc_ipc SHARED)

# Set EXPORT for compiling this target's own sources (PRIVATE)
target_compile_definitions(rbc_ipc PRIVATE RBC_IPC_API=LUISA_DECLSPEC_DLL_EXPORT)
# Set IMPORT for other targets that link to this target (PUBLIC)
target_compile_definitions(rbc_ipc PUBLIC RBC_IPC_API=LUISA_DECLSPEC_DLL_IMPORT)

# Apply basic settings
rbc_apply_basic_settings(rbc_ipc PROJECT_KIND shared ENABLE_EXCEPTION)
rbc_set_precompiled_header(rbc_ipc ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Collect source files
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)
target_sources(rbc_ipc PRIVATE ${SOURCES})

# Public interface: headers that users of this library need
target_include_directories(rbc_ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Private dependencies: only needed for building this library
target_link_libraries(rbc_ipc PRIVATE
    cpp-ipc
    rbc_core
)

