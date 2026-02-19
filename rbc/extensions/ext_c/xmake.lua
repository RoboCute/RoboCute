target("rbc_ext_c")
do
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true
    })

    add_deps('rbc_core', 'test_graphics')
    add_deps('rbc_render_plugin', 'lc-backends-dummy', {
        inherit = false,
        links = false
    })
    set_extension('.pyd')
    add_rules('pybind')
    add_deps('rbc_ext_common')
    add_files("src/**.cpp")
    rbc_set_pch('src/zz_pch.h')
end
target_end()