target('rbc_runtime')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_includedirs('include', {
    public = true
})
add_files('src/**.cpp')
add_deps('rbc_core')
add_defines('RBC_RUNTIME_EXPORT_DLL')
target_end()
