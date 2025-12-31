# Test Resource

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

add_executable(test_resource
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

rbc_apply_basic_settings(test_resource PROJECT_KIND binary)

target_link_libraries(test_resource PRIVATE
    rbc_runtime
    rbc_core
)

