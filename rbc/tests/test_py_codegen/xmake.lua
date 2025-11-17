target('test_py_codegen')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_deps('nanobind', 'lc-core')
set_extension('.pyd')
-- add_rules('py_codegen', {
--     root_path = os.scriptdir()
-- })
-- add_files('*.py')
add_files('builtin/*.cpp')
add_includedirs('builtin')
set_pcxxheader('builtin/zz_pch.h')
add_rules('py_codegen', {
    compile_path = path.join(os.scriptdir(), 'generated/*.cpp')
})
on_load(function(target)
    target:add('files', path.join(os.projectdir(), 'scripts/py/test_codegen.py'))
end)
target_end()

target('test_py_codegen_impl')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_deps('lc-core')
add_files('impl/*.cpp')
add_includedirs('.', 'builtin', 'generated')
target_end()