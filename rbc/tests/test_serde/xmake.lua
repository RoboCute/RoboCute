target('test_serde')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
add_files('**.cpp')
add_deps('rbc_runtime', 'rbc_world_v2')
target_end()
