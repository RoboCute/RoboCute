# RBC Editor Runtime

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# Qt setup - ensures proper include paths and MOC configuration
qt6_standard_project_setup()

# Editor Runtime Library
add_library(rbc_editor_runtime SHARED)

rbc_apply_basic_settings(rbc_editor_runtime PROJECT_KIND shared ENABLE_EXCEPTION RTTI)

# Include directories
target_include_directories(rbc_editor_runtime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Headers (for Qt MOC)
file(GLOB_RECURSE HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/RBCEditorRuntime/*.h
)

# Sources
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

target_sources(rbc_editor_runtime PRIVATE
    ${SOURCES}
    ${HEADERS}
)

# Precompiled header
rbc_set_precompiled_header(rbc_editor_runtime ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Qt resources
qt6_add_resources(QRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rbc_editor.qrc)
target_sources(rbc_editor_runtime PRIVATE ${QRC_FILES})

# Qt MOC
set_target_properties(rbc_editor_runtime PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# Qt libraries
target_link_libraries(rbc_editor_runtime PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::GuiPrivate  # Required for Qt RHI (Render Hardware Interface) private APIs
    Qt6::Widgets
    Qt6::Network
)

# Dependencies
# Note: rbc_core, rbc_runtime, rbc_render_plugin, rbc_app are PUBLIC because
# rbc_editor (main.cpp) directly includes headers from these modules
target_link_libraries(rbc_editor_runtime PUBLIC
    rbc_core
    rbc_runtime
    rbc_render_plugin
    rbc_app
)

target_link_libraries(rbc_editor_runtime PRIVATE
    lc-volk
    qt_node_editor
)

# Compile definitions
target_compile_definitions(rbc_editor_runtime PRIVATE
    LUISA_QT_SAMPLE_ENABLE_DX
    LUISA_QT_SAMPLE_ENABLE_VK
    RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT
)

