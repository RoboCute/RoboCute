target('rbc_render_plugin')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
set_pcxxheader('src/zz_pch.h')
add_deps('rbc_runtime')
add_includedirs('include', '../shader/host', {
    public = true
})
add_files('src/**.cpp')
target_end()
