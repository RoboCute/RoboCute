target('model_viewer')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
set_group('03.samples')
add_deps('rbc_runtime')
add_files('*.cpp')
target_end()

