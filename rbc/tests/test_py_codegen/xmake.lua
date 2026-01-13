target('test_py_codegen')
do
    -- deps LuisaCompute
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true,
        rtti = true
    })
    add_deps('rbc_core', 'test_graphics')
    add_deps('rbc_render_plugin', 'lc-backends-dummy', {inherit = false, links = false})

    set_extension('.pyd')
    add_files('builtin/**.cpp', 'generated/**.cpp', 'main.cpp')
    
    add_includedirs('builtin')

    rbc_set_pch('builtin/zz_pch.h')
    add_rules('pybind')
end
target_end()