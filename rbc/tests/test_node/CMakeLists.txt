# Test Node

include(${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake/rbc_config.cmake)

file(GLOB TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

add_executable(test_node ${TEST_SOURCES})

rbc_apply_basic_settings(test_node PROJECT_KIND binary)

target_link_libraries(test_node PRIVATE
    rbc_node
    rbc_runtime
    rbc_core
)

# Link LuisaCompute targets for DSL support
if(TARGET lc-core)
    target_link_libraries(test_node PRIVATE lc-core)
elseif(TARGET luisa-compute-core)
    target_link_libraries(test_node PRIVATE luisa-compute-core)
endif()

# Link DSL library (required for DSL features like Kernel1D, BufferVar, etc.)
if(TARGET lc-dsl)
    target_link_libraries(test_node PRIVATE lc-dsl)
elseif(TARGET luisa-compute-dsl)
    target_link_libraries(test_node PRIVATE luisa-compute-dsl)
endif()

if(TARGET lc-runtime)
    target_link_libraries(test_node PRIVATE lc-runtime)
elseif(TARGET luisa-compute-runtime)
    target_link_libraries(test_node PRIVATE luisa-compute-runtime)
endif()

if(TARGET luisa-compute-include)
    target_link_libraries(test_node PRIVATE luisa-compute-include)
endif()

