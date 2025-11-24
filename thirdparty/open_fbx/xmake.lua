target('open_fbx')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
add_files('src/*.cpp')
add_includedirs('src', {
    public = true
})
target_end()