target('test_py_codegen')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_deps('nanobind', 'lc-core')
set_extension('.pyd')
add_files('builtin/*.cpp', 'generated/*.cpp', 'main.cpp')
add_includedirs('builtin')
set_pcxxheader('builtin/zz_pch.h')
-- add_rules('py_codegen', {
--     compile_path = path.join(os.scriptdir(), 'generated/*.cpp')
-- })
target_end()

target('py_backend_impl')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_deps('lc-core')
add_files('impl/*.cpp')
add_includedirs('.', 'builtin', 'generated')
target_end()