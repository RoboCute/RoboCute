target('rbc_render_interface')
set_kind('headeronly')
add_includedirs('include', { public = true})
target_end()

target('rbc_render_plugin')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
set_pcxxheader('src/zz_pch.h')
add_deps('rbc_runtime')
add_deps("compile_shaders_hostgen", {
    inherit = false
})
add_deps('rbc_render_interface', 'oidn_plugin')
add_includedirs('../shader/host', {
    public = true
})
add_deps('lc-backends-dummy', {inherit = false, links = false})
add_files('src/**.cpp')
target_end()
