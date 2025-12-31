# Test IPC

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

# Collect source files
file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_ipc ${TEST_SOURCES})

rbc_apply_basic_settings(test_ipc PROJECT_KIND binary)

target_link_libraries(test_ipc PRIVATE
    rbc_ipc
    rbc_core
)

