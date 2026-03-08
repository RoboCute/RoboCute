target('tinystl')
add_rules('lc_basic_settings', {
    project_kind = 'headeronly'
})
add_includedirs('tinystl/modules/core/include', {
    public = true
})
target_end()
