# RBC Python Extension (Pybind)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_interface_target.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Find Python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# Find pybind11 (from LuisaCompute)
set(PYBIND11_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/LuisaCompute/src/ext/pybind11/include)
if(NOT EXISTS ${PYBIND11_INCLUDE_DIR})
    message(FATAL_ERROR "pybind11 not found at ${PYBIND11_INCLUDE_DIR}")
endif()

# Interface callback function
function(rbc_ext_c_interface_callback target)
    target_include_directories(${target} INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endfunction()

# Implementation callback function
function(rbc_ext_c_impl_callback target)
    rbc_apply_basic_settings(${target} PROJECT_KIND shared ENABLE_EXCEPTION RTTI)
    
    # Set extension to .pyd on Windows
    if(WIN32)
        set_target_properties(${target} PROPERTIES SUFFIX ".pyd")
    endif()
    
    # Sources
    file(GLOB SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    )
    target_sources(${target} PRIVATE ${SOURCES})
    
    # Include directories
    target_include_directories(${target} PRIVATE
        ${Python3_INCLUDE_DIRS}
        ${PYBIND11_INCLUDE_DIR}
    )
    
    # Link libraries
    target_link_libraries(${target} PRIVATE
        lc-core
        ${Python3_LIBRARIES}
    )
    
    # Link directories
    if(Python3_LIBRARY_DIRS)
        target_link_directories(${target} PRIVATE ${Python3_LIBRARY_DIRS})
    endif()
    
    # Windows-specific: /bigobj flag
    if(MSVC)
        target_compile_options(${target} PRIVATE /bigobj)
    endif()
endfunction()

# Create interface target
rbc_interface_target(rbc_ext_c
    INTERFACE_CALLBACK rbc_ext_c_interface_callback
    IMPL_CALLBACK rbc_ext_c_impl_callback
)

