target('sample_anim')
add_rules('lc_basic_settings', {
    project_kind = 'binary'
})
add_files('**.cpp')
add_deps('rbc_app', 'RBCTracy', 'lc-gui')

target_end()
