# Qt Node Editor CMake Configuration

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets OpenGL)

# Qt Node Editor (shared library)
add_library(qt_node_editor SHARED)
file(GLOB QT_NODE_EDITOR_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor/src/*.cpp
)
file(GLOB QT_NODE_EDITOR_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor/include/QtNodes/internal/*.hpp
)

target_sources(qt_node_editor PRIVATE
    ${QT_NODE_EDITOR_SOURCES}
    ${QT_NODE_EDITOR_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor/resources/resources.qrc
)

target_include_directories(qt_node_editor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor/include
)
target_include_directories(qt_node_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor/src
    ${CMAKE_CURRENT_SOURCE_DIR}/qt_node_editor/include/QtNodes/internal
)

target_link_libraries(qt_node_editor PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
)

target_compile_definitions(qt_node_editor PUBLIC NODE_EDITOR_SHARED)
target_compile_definitions(qt_node_editor PRIVATE NODE_EDITOR_EXPORTS QT_NO_KEYWORDS)

# Enable Qt MOC
set_target_properties(qt_node_editor PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)

rbc_apply_basic_settings(qt_node_editor PROJECT_KIND shared)

