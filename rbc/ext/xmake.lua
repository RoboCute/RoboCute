target('rbc_ext')
    -- deps LuisaCompute
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_deps('nanobind')
    set_extension('.pyd')
    add_deps("rbc_world")

    add_includedirs('builtin')
    add_files(
        "*.cpp",
        "builtin/*.cpp"
    )

target_end()
