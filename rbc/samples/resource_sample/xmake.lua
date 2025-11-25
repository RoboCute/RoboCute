target('resource_sample')
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    add_files('*.cpp')
    add_deps('rbc_world')
target_end()

