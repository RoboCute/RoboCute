# RBC Runtime Library

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_interface_target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Interface callback function
function(rbc_runtime_interface_callback target)
    target_include_directories(${target} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../shader/include
    )
    target_link_libraries(${target} INTERFACE
        lc-runtime
        rbc_core
    )
endfunction()

# Implementation callback function
function(rbc_runtime_impl_callback target)
    rbc_apply_basic_settings(${target} PROJECT_KIND shared)
    rbc_enable_unity_build(${target} 8)
    rbc_set_precompiled_header(${target} ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)
    
    file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )
    target_sources(${target} PRIVATE ${SOURCES})
    
    # Dependencies
    target_link_libraries(${target} PRIVATE
        tinyexr
        tiny_obj_loader
        open_fbx
        tinytiff
        tinygltf
        ozz_animation_runtime_static
        ozz_animation_offline_static
        lc-volk
    )
    
    # Note: lc-backends-dummy is handled by LuisaCompute (no link needed)
    # stb-image is header-only, provided by LuisaCompute
    
    # Windows-specific system libraries
    if(WIN32)
        target_link_libraries(${target} PRIVATE d3d12 User32)
    endif()
    
    # Export macro
    target_compile_definitions(${target} PRIVATE RBC_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT)
endfunction()

# Create interface target
rbc_interface_target(rbc_runtime
    INTERFACE_CALLBACK rbc_runtime_interface_callback
    IMPL_CALLBACK rbc_runtime_impl_callback
)

# Add import definition to interface
target_compile_definitions(rbc_runtime_int INTERFACE RBC_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT)

# Add ozz_animation_include dependency
target_link_libraries(rbc_runtime_int INTERFACE ozz_animation_include)

