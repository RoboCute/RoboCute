target('lib3ds')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
add_files('lib3ds/lib3ds/*.c')
add_includedirs('lib3ds', {
    public = true
})
target_end()
