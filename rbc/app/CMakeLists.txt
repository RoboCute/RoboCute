# RBC App Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_interface_target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Interface callback function
function(rbc_app_interface_callback target)
    target_include_directories(${target} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(${target} INTERFACE
        rbc_runtime
        rbc_core
        rbc_render_plugin
    )
endfunction()

# Implementation callback function
function(rbc_app_impl_callback target)
    rbc_apply_basic_settings(${target} PROJECT_KIND shared)
    rbc_set_precompiled_header(${target} ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)
    
    file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )
    target_sources(${target} PRIVATE ${SOURCES})
    
    target_link_libraries(${target} PRIVATE
        lc-gui
    )
    
    # Export macro
    target_compile_definitions(${target} PRIVATE RBC_APP_API=LUISA_DECLSPEC_DLL_EXPORT)
endfunction()

# Create interface target
rbc_interface_target(rbc_app
    INTERFACE_CALLBACK rbc_app_interface_callback
    IMPL_CALLBACK rbc_app_impl_callback
)

# Add import definition to interface
target_compile_definitions(rbc_app_int INTERFACE RBC_APP_API=LUISA_DECLSPEC_DLL_IMPORT)

