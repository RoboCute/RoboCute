# RBC Modules

message(STATUS "=== Building RBC Modules ===")

# Core libraries
message(STATUS "Including core library...")
add_subdirectory(core)
message(STATUS "Including runtime library...")
add_subdirectory(runtime)
message(STATUS "Including ipc library...")
add_subdirectory(ipc)
message(STATUS "Including app library...")
add_subdirectory(app)
message(STATUS "Including node_graph library...")
add_subdirectory(node_graph)

# Plugins
message(STATUS "Including render_plugin...")
add_subdirectory(render_plugin)
message(STATUS "Including oidn_plugin...")
add_subdirectory(oidn_plugin)

# Editor (only if enabled)
if(RBC_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

# Extensions (Python extension - only if enabled)
if(RBC_BUILD_PY_EXT)
    add_subdirectory(ext_c)
else()
    message(STATUS "Skipping ext_c (RBC_BUILD_PY_EXT=OFF)")
endif()

# Tests
message(STATUS "Including tests...")
add_subdirectory(tests)

