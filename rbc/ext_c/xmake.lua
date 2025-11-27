target('rbc_ext_c')
    -- deps LuisaCompute
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true,
        rtti = true
    })
    add_deps('nanobind')
    set_extension('.pyd')
    add_deps("rbc_world")

    add_includedirs('builtin')
    add_files("**.cpp")
target_end()
