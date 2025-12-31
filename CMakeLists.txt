cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(Robocute VERSION 0.1.0 LANGUAGES CXX)

# Include RBC CMake configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/rbc_config.cmake)

# Luisa Compute
set(LuisaCompute_SOURCE_DIR "thirdparty/LuisaCompute")
message(STATUS "LuisaCompute source dir: ${LuisaCompute_SOURCE_DIR}")
include(${LuisaCompute_SOURCE_DIR}/scripts/setup_output_dirs.cmake)
set(LUISA_COMPUTE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_DOWNLOAD_NVCOMP OFF CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_USE_SYSTEM_STL ON CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_ENABLE_RUST OFF CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_ENABLE_CPU OFF CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_ENABLE_REMOTE OFF CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_ENABLE_GUI ON CACHE BOOL "" FORCE)
set(LUISA_COMPUTE_ENABLE_WAYLAND ON CACHE BOOL "" FORCE)
add_subdirectory(${LuisaCompute_SOURCE_DIR})

# Qt6
find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets Core Gui Network OpenGL)
qt_standard_project_setup()

# Third-party libraries
add_subdirectory(thirdparty)

# RBC modules
add_subdirectory(rbc)