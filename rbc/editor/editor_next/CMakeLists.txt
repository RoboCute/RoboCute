# RBC Editor Next (QML Version)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

# Find Qt6 components required for QML/Quick application
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Gui 
    Widgets 
    Qml 
    Quick 
    QuickControls2 
    Network
)

# Qt setup - ensures proper include paths and MOC configuration
qt6_standard_project_setup()

# Editor Next executable
add_executable(rbc_editor_next
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Apply basic settings (equivalent to lc_basic_settings)
rbc_apply_basic_settings(rbc_editor_next PROJECT_KIND binary ENABLE_EXCEPTION RTTI)

# Include directories
target_include_directories(rbc_editor_next PRIVATE
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

target_sources(rbc_editor_next PRIVATE
    ${SOURCES}
    ${HEADERS}
)

# Precompiled header
rbc_set_precompiled_header(rbc_editor_next ${CMAKE_CURRENT_SOURCE_DIR}/src/zz_pch.h)

# Qt resources (includes QML files)
qt6_add_resources(QRC_FILES ${CMAKE_CURRENT_SOURCE_DIR}/rbc_editor.qrc)
target_sources(rbc_editor_next PRIVATE ${QRC_FILES})

# Qt MOC and RCC
set_target_properties(rbc_editor_next PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

# Windows subsystem (GUI application)
if(WIN32)
    set_target_properties(rbc_editor_next PROPERTIES
        WIN32_EXECUTABLE ON
    )
endif()

# Qt libraries
target_link_libraries(rbc_editor_next PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
)

# Dependencies
target_link_libraries(rbc_editor_next PRIVATE
    rbc_core
    rbc_runtime
    rbc_render_plugin
    rbc_app
    lc-volk
    qt_node_editor
)

# Compile definitions
target_compile_definitions(rbc_editor_next PRIVATE
    LUISA_QT_SAMPLE_ENABLE_DX
    LUISA_QT_SAMPLE_ENABLE_VK
    RBC_EDITOR_RUNTIME_API=
)

# Enable QML debugging in Debug mode
# This allows Qt Creator to connect to the QML debugger
target_compile_definitions(rbc_editor_next PRIVATE
    $<$<CONFIG:Debug>:QT_QML_DEBUG>
)

