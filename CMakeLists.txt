cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Enable C language for LuisaCompute dependencies (e.g., yyjson)
set(CMAKE_C_STANDARD_REQUIRED ON)
project(Robocute VERSION 0.1.0)

# Build options
option(RBC_BUILD_EDITOR "Build the editor (requires Qt6)" ON)
option(RBC_BUILD_PY_EXT "Build Python extension (requires Python3)" OFF)

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

# Qt6 (only required if building editor)
if(RBC_BUILD_EDITOR)
    # Allow user to override Qt6 path via command line: cmake -DQt6_DIR=... or -DQt6_ROOT=...
    # Qt6_ROOT should point to Qt installation root (e.g., D:/tools/Qt/6.9.3)
    # Qt6_DIR should point to cmake config directory (e.g., D:/tools/Qt/6.9.3/lib/cmake/Qt6)

    if(Qt6_ROOT)
        # User specified Qt6_ROOT, convert to Qt6_DIR
        set(Qt6_DIR "${Qt6_ROOT}/lib/cmake/Qt6" CACHE PATH "Qt6 cmake config directory")
        list(APPEND CMAKE_PREFIX_PATH "${Qt6_ROOT}")
        message(STATUS "Using Qt6_ROOT: ${Qt6_ROOT}")
    elseif(NOT Qt6_DIR)
        # Try to find Qt6 in common installation paths
        set(QT_POSSIBLE_PATHS
            "D:/tools/Qt/6.9.3"
            "C:/Qt/6.9.3"
            "$ENV{QTDIR}"
            "$ENV{QT_DIR}"
        )
        
        foreach(QT_PATH ${QT_POSSIBLE_PATHS})
            if(EXISTS "${QT_PATH}/lib/cmake/Qt6/Qt6Config.cmake")
                set(Qt6_DIR "${QT_PATH}/lib/cmake/Qt6" CACHE PATH "Qt6 cmake config directory")
                list(APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
                message(STATUS "Found Qt6 at: ${Qt6_DIR}")
                break()
            endif()
        endforeach()
        
        # If still not found, try adding to CMAKE_PREFIX_PATH
        if(NOT Qt6_DIR)
            foreach(QT_PATH ${QT_POSSIBLE_PATHS})
                if(EXISTS "${QT_PATH}")
                    list(APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
                    message(STATUS "Added to CMAKE_PREFIX_PATH: ${QT_PATH}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    # If Qt6_DIR is set, ensure parent directory is in CMAKE_PREFIX_PATH
    if(Qt6_DIR)
        get_filename_component(QT_ROOT "${Qt6_DIR}/../.." ABSOLUTE)
        if(EXISTS "${QT_ROOT}")
            list(APPEND CMAKE_PREFIX_PATH "${QT_ROOT}")
        endif()
    endif()

    find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Core Gui Network OpenGL)
    if(Qt6_FOUND)
        message(STATUS "Qt6 found: ${Qt6_VERSION}")
        message(STATUS "Qt6_DIR: ${Qt6_DIR}")
    else()
        message(FATAL_ERROR "Qt6 not found. Please set Qt6_ROOT or Qt6_DIR:\n"
            "  cmake -DQt6_ROOT=D:/tools/Qt/6.9.3 ..\n"
            "  or\n"
            "  cmake -DQt6_DIR=D:/tools/Qt/6.9.3/lib/cmake/Qt6 ..\n"
            "  or disable editor: cmake -DRBC_BUILD_EDITOR=OFF ..")
    endif()
    qt_standard_project_setup()
else()
    message(STATUS "Editor build disabled (RBC_BUILD_EDITOR=OFF)")
endif()

# Third-party libraries
add_subdirectory(thirdparty)

# RBC modules
add_subdirectory(rbc)