# Test Coro

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

add_executable(test_coro
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

rbc_apply_basic_settings(test_coro PROJECT_KIND binary)

target_link_libraries(test_coro PRIVATE
    rbc_core
)

