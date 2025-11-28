nested_target('test_py_codegen', 'rbc_render_plugin', 'lc-backends-dummy')
do
    -- deps LuisaCompute
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true,
        rtti = true
    })
    add_deps('rbc_core', 'test_graphics')
    set_extension('.pyd')
    add_files('builtin/*.cpp', 'generated/*.cpp', 'main.cpp')
    add_includedirs('builtin')
    set_pcxxheader('builtin/zz_pch.h')
    add_rules('pybind')
end
target_end()

-- target('py_backend_impl')
-- do
--     add_rules('lc_basic_settings', {
--         project_kind = 'shared'
--     })
--     add_deps('rbc_core')
--     add_files('impl/*.cpp')
--     add_includedirs('.', 'builtin', 'generated')
-- end
-- target_end()
