target('test_serde')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
set_group("03.samples")
add_files('**.cpp')
add_deps('rbc_runtime', 'rbc_node')

target_end()
