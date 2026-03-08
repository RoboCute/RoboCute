target('lwo')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
add_files('lwo/*.c')
add_files('lwo/*.cpp')
add_includedirs('lwo', {
    public = true
})
target_end()
