# Test Python Codegen

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_py_codegen ${SOURCES})

rbc_apply_basic_settings(test_py_codegen PROJECT_KIND binary)

target_include_directories(test_py_codegen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/builtin
)

target_link_libraries(test_py_codegen PRIVATE
    rbc_core
)

