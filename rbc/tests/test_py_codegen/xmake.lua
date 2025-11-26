target('test_py_codegen')
do
    -- deps LuisaCompute
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true,
        rtti = true,
        toolchain = get_config('rbc_py_toolchain')
    })
    add_deps('nanobind')
    add_deps('rbc_core')
    set_extension('.pyd')
    add_files('builtin/*.cpp', 'generated/*.cpp', 'main.cpp')
    add_includedirs('builtin')
    set_pcxxheader('builtin/zz_pch.h')
    on_load(function(target)
        target:add("cxflags", "/bigobj", {
            tools = "cl",
            public = true
        })
        target:add('includedirs', path.join(get_config('rbc_pybind_path'), 'include'))
    end)
end
target_end()

target('py_backend_impl')
do
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_deps('rbc_core')
    add_files('impl/*.cpp')
    add_includedirs('.', 'builtin', 'generated')
end
target_end()
