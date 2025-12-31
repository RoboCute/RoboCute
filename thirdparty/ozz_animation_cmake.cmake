# Ozz Animation CMake Configuration

# Ozz Animation Include
add_library(ozz_animation_include INTERFACE)
target_include_directories(ozz_animation_include INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/include
)

# Ozz Animation Base
add_library(ozz_animation_base STATIC)
file(GLOB OZZ_BASE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/base/*.cc
)
target_sources(ozz_animation_base PRIVATE ${OZZ_BASE_SOURCES})
target_link_libraries(ozz_animation_base PUBLIC ozz_animation_include)
rbc_apply_basic_settings(ozz_animation_base PROJECT_KIND static)

# Ozz Animation Runtime Static
add_library(ozz_animation_runtime_static STATIC)
file(GLOB OZZ_RUNTIME_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/animation/runtime/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/geometry/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/options/*.cc
)
target_sources(ozz_animation_runtime_static PRIVATE ${OZZ_RUNTIME_SOURCES})
target_include_directories(ozz_animation_runtime_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src
)
target_link_libraries(ozz_animation_runtime_static PUBLIC ozz_animation_base)
target_compile_definitions(ozz_animation_runtime_static PRIVATE OZZ_BUILD_ANIMATION_LIB)
rbc_apply_basic_settings(ozz_animation_runtime_static PROJECT_KIND static)

# Ozz JSON CPP (shared)
add_library(ozz_json_cpp SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/extern/jsoncpp/dist/jsoncpp.cpp
)
target_include_directories(ozz_json_cpp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/extern/jsoncpp/dist
)
target_compile_definitions(ozz_json_cpp PRIVATE JSON_DLL_BUILD)
target_compile_definitions(ozz_json_cpp PUBLIC JSON_DLL)
rbc_apply_basic_settings(ozz_json_cpp PROJECT_KIND shared)

# Ozz Animation Offline Static
add_library(ozz_animation_offline_static STATIC)
file(GLOB OZZ_OFFLINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/animation/offline/*.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/animation/offline/tools/*.cc
)
target_sources(ozz_animation_offline_static PRIVATE ${OZZ_OFFLINE_SOURCES})
target_include_directories(ozz_animation_offline_static PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src
    ${CMAKE_CURRENT_SOURCE_DIR}/ozz_animation/src/animation/offline/tools
)
target_link_libraries(ozz_animation_offline_static PUBLIC ozz_animation_base)
target_link_libraries(ozz_animation_offline_static PRIVATE ozz_json_cpp)
rbc_apply_basic_settings(ozz_animation_offline_static PROJECT_KIND static)

