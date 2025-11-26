tool_target('test_window_hook')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
add_files('**.cpp')
add_deps('lc-gui', 'lc-vstl', 'rbc_ipc', 'rbc_core')
target_end()
