# Test Graphics

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_shader_compile.cmake)

# Test Graphics (shared library)
add_library(test_graphics SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/world_scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/world_scene.h
    ${CMAKE_CURRENT_SOURCE_DIR}/interface.cpp
)

rbc_apply_basic_settings(test_graphics PROJECT_KIND shared)

target_include_directories(test_graphics PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_link_libraries(test_graphics PRIVATE
    rbc_app
    rbc_render_plugin
    rbc_ipc
    lc-gui
)

target_compile_definitions(test_graphics PRIVATE TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_EXPORT)
target_compile_definitions(test_graphics PUBLIC TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_IMPORT)

# Add shader compilation dependency
rbc_add_shader_compilation(test_graphics)

# Test Graphics Binary (standalone executable)
add_executable(test_graphics_bin
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/world_scene.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/world_scene.h
)

rbc_apply_basic_settings(test_graphics_bin PROJECT_KIND binary)

target_include_directories(test_graphics_bin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/generated
)

target_link_libraries(test_graphics_bin PRIVATE
    rbc_app
    rbc_render_plugin
    rbc_ipc
    lc-gui
)

target_compile_definitions(test_graphics_bin PRIVATE STANDALONE)

# Add shader compilation dependency
rbc_add_shader_compilation(test_graphics_bin)

