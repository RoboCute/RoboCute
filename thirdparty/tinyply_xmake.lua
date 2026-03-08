target('tinyply')
add_rules('lc_basic_settings', {
    project_kind = 'static',
    enable_exception = true
})
add_files('tinyply/source/tinyply.cpp')
add_includedirs('tinyply/source', {
    public = true
})
target_end()
