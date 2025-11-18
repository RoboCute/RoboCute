target('rbc_core')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_includedirs('include', {
    public = true
})
add_files('src/**.cpp')
add_deps('lc-core', 'lc-yyjson', 'lc-vstl')
add_defines('RBC_CORE_EXPORT_DLL')
target_end()
