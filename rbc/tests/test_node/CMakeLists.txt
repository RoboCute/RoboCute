# Test Node

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

add_executable(test_node
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

rbc_apply_basic_settings(test_node PROJECT_KIND binary)

target_link_libraries(test_node PRIVATE
    rbc_node
)

