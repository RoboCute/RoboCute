target('rbc_ext')
    -- deps LuisaCompute
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_deps("rbc_world")
target_end()
