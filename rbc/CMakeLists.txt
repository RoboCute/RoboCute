# RBC Modules

# Core libraries
add_subdirectory(core)
add_subdirectory(runtime)
add_subdirectory(ipc)
add_subdirectory(app)
add_subdirectory(node_graph)

# Plugins
add_subdirectory(render_plugin)
add_subdirectory(oidn_plugin)

# Editor
add_subdirectory(editor)

# Extensions
add_subdirectory(ext_c)

# Tests
add_subdirectory(tests)

