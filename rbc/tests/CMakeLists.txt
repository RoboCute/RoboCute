# RBC Tests

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/rbc_config.cmake)

# Helper function to add a test
function(rbc_add_test test_name deps)
    add_executable(test_${test_name}
        ${CMAKE_CURRENT_SOURCE_DIR}/${test_name}/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/_framework/test_util.cpp
    )
    
    rbc_apply_basic_settings(test_${test_name} PROJECT_KIND binary ENABLE_EXCEPTION)
    
    target_include_directories(test_${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/_framework
    )
    
    target_link_libraries(test_${test_name} PRIVATE
        external_doctest
        ${deps}
    )
    
    # Enable CTest
    add_test(NAME ${test_name} COMMAND test_${test_name})
endfunction()

# Add simple tests
rbc_add_test("core" "rbc_core")
rbc_add_test("world" "rbc_runtime;rbc_core")
rbc_add_test("anim" "rbc_runtime;rbc_core")

# Add subdirectories for more complex tests
add_subdirectory(test_ipc)
add_subdirectory(test_graphics)
add_subdirectory(test_node)
add_subdirectory(test_serde)
add_subdirectory(test_resource)
add_subdirectory(test_coro)
add_subdirectory(sample_anim)
add_subdirectory(model_viewer)
add_subdirectory(test_py_codegen)
add_subdirectory(calculator)

