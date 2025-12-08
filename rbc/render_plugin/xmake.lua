local function rbc_render_interface()
    add_includedirs('include', {
        public = true
    })
end
local function rbc_render_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    set_pcxxheader('src/zz_pch.h')
    add_deps('rbc_runtime')
    add_deps("compile_shaders_hostgen", {
        inherit = false
    })
    add_deps('oidn_plugin', {
        links = false
    })
    add_includedirs('../shader/host', {
        public = true
    })
    add_deps('lc-backends-dummy', {
        inherit = false,
        links = false
    })
    add_files('src/**.cpp')
end

interface_target('rbc_render_plugin', rbc_render_interface, rbc_render_impl, true)
