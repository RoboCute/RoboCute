target('tiny_obj_loader')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
add_files('*.cpp')
add_includedirs('.', {
    public = true
})
target_end()
