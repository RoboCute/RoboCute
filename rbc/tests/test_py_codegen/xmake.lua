target('test_py_codegen')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_deps('nanobind', 'rbc_core')
set_extension('.pyd')
add_files('builtin/*.cpp', 'generated/*.cpp', 'main.cpp')
add_includedirs('builtin')
set_pcxxheader('builtin/zz_pch.h')
-- add_rules('py_codegen', {
--     compile_path = path.join(os.scriptdir(), 'generated/*.cpp')
-- })
add_rules('py_stubgen', {
    stubgen_path = 'scripts/generated'
})
target_end()

target('py_backend_impl')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})

add_deps('rbc_core')
add_files('impl/*.cpp')
add_includedirs('.', 'builtin', 'generated')
target_end()
