# Third-party Libraries CMake Configuration

# Include helper modules
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/rbc_config.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/rbc_interface_target.cmake)

# LuisaCompute - already handled in root CMakeLists.txt
# We just need to create an interface target for dll_export.h
add_library(rbc_dllexport_include INTERFACE)
target_include_directories(rbc_dllexport_include INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/LuisaCompute/include
)

# RTM - header only
add_library(rtm INTERFACE)
target_include_directories(rtm INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/rtm/include
)
target_compile_definitions(rtm INTERFACE RTM_ON_ASSERT_ABORT)

# TinyOBJLoader - static library
add_library(tiny_obj_loader STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tiny_obj_loader/tiny_obj_loader.cpp
)
target_include_directories(tiny_obj_loader PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tiny_obj_loader
)
rbc_apply_basic_settings(tiny_obj_loader PROJECT_KIND static)

# TinyEXR - static library
add_library(tinyexr STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyexr/tinyexr.cpp
)
target_include_directories(tinyexr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyexr
)
rbc_apply_basic_settings(tinyexr PROJECT_KIND static)

# TinyTIFF - static library
file(GLOB TINYTIFF_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tinytiff/*.c
)
add_library(tinytiff STATIC ${TINYTIFF_SOURCES})
target_include_directories(tinytiff PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tinytiff
)
rbc_apply_basic_settings(tinytiff PROJECT_KIND static)
if(WIN32)
    target_compile_definitions(tinytiff PRIVATE HAVE_FSEEKI64)
endif()

# TinyGLTF - static library
file(GLOB TINYGLTF_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf/tiny_gltf.cc
)
add_library(tinygltf STATIC ${TINYGLTF_SOURCES})
target_include_directories(tinygltf PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tinygltf
)
rbc_apply_basic_settings(tinygltf PROJECT_KIND static)

# OpenFBX - static library
file(GLOB OPENFBX_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/open_fbx/src/*.cpp
)
add_library(open_fbx STATIC ${OPENFBX_SOURCES})
target_include_directories(open_fbx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/open_fbx/src
)
rbc_apply_basic_settings(open_fbx PROJECT_KIND static)

# cpp-ipc - static library
file(GLOB CPPIPC_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-ipc/src/libipc/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-ipc/src/libipc/sync/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-ipc/src/libipc/platform/platform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-ipc/src/libipc/platform/platform.c
)
add_library(cpp-ipc STATIC ${CPPIPC_SOURCES})
target_include_directories(cpp-ipc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-ipc/include
)
target_include_directories(cpp-ipc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-ipc/src
)
rbc_apply_basic_settings(cpp-ipc PROJECT_KIND static ENABLE_EXCEPTION)
if(WIN32)
    target_compile_definitions(cpp-ipc PRIVATE IPC_OS_WINDOWS_)
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(cpp-ipc PRIVATE IPC_OS_LINUX_)
endif()

# Doctest - header only
add_library(external_doctest INTERFACE)
target_include_directories(external_doctest INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/doctest/include
)

# Ozz Animation
include(${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation_cmake.cmake)

# Tracy
include(${CMAKE_CURRENT_SOURCE_DIR}/tracy_cmake.cmake)

# Qt Node Editor
include(${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor_cmake.cmake)

# OIDN Interface (header only)
add_library(oidn_interface INTERFACE)
target_include_directories(oidn_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/oidn
)

