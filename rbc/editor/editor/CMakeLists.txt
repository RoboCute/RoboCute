# RBC Editor Executable

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Editor executable
add_executable(rbc_editor
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

rbc_apply_basic_settings(rbc_editor PROJECT_KIND binary ENABLE_EXCEPTION RTTI)

# Qt setup
qt6_standard_project_setup()

# Qt MOC
set_target_properties(rbc_editor PROPERTIES AUTOMOC ON)

# Dependencies
target_link_libraries(rbc_editor PRIVATE
    rbc_editor_runtime
    Qt6::Widgets
)

# Compile definitions
target_compile_definitions(rbc_editor PRIVATE
    RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT
)

