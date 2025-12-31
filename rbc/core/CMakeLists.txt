# RBC Core Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_interface_target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Interface callback function
function(rbc_core_interface_callback target)
    target_include_directories(${target} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${target} INTERFACE
        lc-core
        lc-vstl
        lc-yyjson
        RBCTracy
    )
endfunction()

# Implementation callback function
function(rbc_core_impl_callback target)
    rbc_apply_basic_settings(${target} PROJECT_KIND shared)
    rbc_enable_unity_build(${target} 4)
    rbc_set_precompiled_header(${target} ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)
    
    file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )
    target_sources(${target} PRIVATE ${SOURCES})
    
    target_link_libraries(${target} PRIVATE rtm)
    
    # Export macro
    target_compile_definitions(${target} PRIVATE RBC_CORE_API=LUISA_DECLSPEC_DLL_EXPORT)
endfunction()

# Create interface target
rbc_interface_target(rbc_core
    INTERFACE_CALLBACK rbc_core_interface_callback
    IMPL_CALLBACK rbc_core_impl_callback
)

# Add import definition to interface
target_compile_definitions(rbc_core_int INTERFACE RBC_CORE_API=LUISA_DECLSPEC_DLL_IMPORT)

