# Tracy CMake Configuration

# Tracy Interface
add_library(RBCTracy_int INTERFACE)
target_include_directories(RBCTracy_int INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tracy/tracy
)
target_compile_definitions(RBCTracy_int INTERFACE RBC_PROFILE_ENABLE)

# Tracy Implementation (static)
add_library(RBCTracy_impl STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/tracy/tracy/TracyClient.cpp
)
target_link_libraries(RBCTracy_impl PRIVATE RBCTracy_int)
target_compile_definitions(RBCTracy_impl PRIVATE TRACY_EXPORTS TRACY_ENABLE)
rbc_apply_basic_settings(RBCTracy_impl PROJECT_KIND static)

# Alias
add_library(RBCTracy ALIAS RBCTracy_impl)

