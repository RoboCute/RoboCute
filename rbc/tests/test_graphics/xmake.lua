tool_target('test_graphics')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
add_files('**.cpp')
add_deps('rbc_runtime', 'lc-gui', 'rbc_render_interface', 'rbc_ipc')
target_end()
