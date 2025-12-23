target('test_ozz')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
add_files('**.cpp')
add_deps('rbc_runtime')
add_deps('ozz_animation_static')

target_end()
