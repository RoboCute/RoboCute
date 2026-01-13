target('test_project')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
set_group("03.samples")
add_files('**.cpp')
add_deps('rbc_runtime', 'lc-dsl', "rbc_project_plugin")
target_end()
