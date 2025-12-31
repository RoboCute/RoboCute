# RBC Render Plugin

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_interface_target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_shader_compile.cmake)

# Interface callback function
function(rbc_render_interface_callback target)
    target_include_directories(${target} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endfunction()

# Implementation callback function
function(rbc_render_impl_callback target)
    rbc_apply_basic_settings(${target} PROJECT_KIND shared)
    rbc_set_precompiled_header(${target} ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)
    
    # Add shader hostgen dependency
    rbc_add_shader_hostgen(${target})
    
    file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )
    target_sources(${target} PRIVATE ${SOURCES})
    
    target_link_libraries(${target} PRIVATE
        rbc_runtime
    )
    
    # Add shader host directory to includes
    target_include_directories(${target} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../shader/host
    )
    
    # Note: oidn_plugin dependency is NO_LINK, so we don't link it
    # lc-backends-dummy is handled by LuisaCompute
endfunction()

# Create interface target with NO_LINK
rbc_interface_target(rbc_render_plugin
    INTERFACE_CALLBACK rbc_render_interface_callback
    IMPL_CALLBACK rbc_render_impl_callback
    NO_LINK
)

