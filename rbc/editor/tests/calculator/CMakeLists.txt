# Calculator Test (Qt Node Editor)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

file(GLOB SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

file(GLOB HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.hpp
)

add_executable(calculator ${SOURCES} ${HEADERS})

rbc_apply_basic_settings(calculator PROJECT_KIND binary RTTI)

qt6_standard_project_setup()

set_target_properties(calculator PROPERTIES AUTOMOC ON)

target_link_libraries(calculator PRIVATE
    qt_node_editor
    Qt6::Widgets
)

