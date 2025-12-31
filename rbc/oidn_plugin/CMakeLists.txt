# RBC OIDN Plugin

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_interface_target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Interface callback function
function(oidn_plugin_interface_callback target)
    target_include_directories(${target} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endfunction()

# Implementation callback function
function(oidn_plugin_impl_callback target)
    rbc_apply_basic_settings(${target} PROJECT_KIND shared)
    
    file(GLOB_RECURSE SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )
    target_sources(${target} PRIVATE ${SOURCES})
    
    target_link_libraries(${target} PRIVATE
        oidn_interface
        lc-vstl
        lc-runtime
    )
    
    # Link OIDN libraries
    # Note: These should be available from build/download/oidn-2.3.3-windows-x64
    find_library(OIDN_LIB OpenImageDenoise
        PATHS
        ${CMAKE_SOURCE_DIR}/build/download/oidn-2.3.3-windows-x64/lib
        ${CMAKE_SOURCE_DIR}/build/download/oidn-2.3.3-windows-x64/lib/x64
        NO_DEFAULT_PATH
    )
    find_library(OIDN_CORE_LIB OpenImageDenoise_core
        PATHS
        ${CMAKE_SOURCE_DIR}/build/download/oidn-2.3.3-windows-x64/lib
        ${CMAKE_SOURCE_DIR}/build/download/oidn-2.3.3-windows-x64/lib/x64
        NO_DEFAULT_PATH
    )
    
    if(OIDN_LIB AND OIDN_CORE_LIB)
        target_link_libraries(${target} PRIVATE ${OIDN_LIB} ${OIDN_CORE_LIB})
    else()
        message(WARNING "OIDN libraries not found. Please ensure OIDN is installed.")
    endif()
endfunction()

# Create interface target with NO_LINK
rbc_interface_target(oidn_plugin
    INTERFACE_CALLBACK oidn_plugin_interface_callback
    IMPL_CALLBACK oidn_plugin_impl_callback
    NO_LINK
)

# OIDN Checker executable
add_executable(oidn_checker
    ${CMAKE_CURRENT_SOURCE_DIR}/oidn_checker.cpp
)
rbc_apply_basic_settings(oidn_checker PROJECT_KIND binary)
target_link_libraries(oidn_checker PRIVATE
    oidn_plugin
    lc-volk
    lc-runtime
)
# Note: lc-backends-dummy dependency is handled by LuisaCompute

