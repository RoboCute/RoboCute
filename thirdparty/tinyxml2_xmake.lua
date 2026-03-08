target('tinyxml2')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
add_files('tinyxml2/tinyxml2.cpp')
add_includedirs('tinyxml2', {
    public = true
})
target_end()
