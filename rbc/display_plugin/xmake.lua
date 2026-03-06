local function rbc_display_interface()
    add_includedirs('include', {
        public = true
    })
end
local function rbc_display_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    rbc_set_pch('src/zz_pch.h')
    add_deps('rbc_runtime')
    add_includedirs('include')
    add_syslinks('dwmapi', 'user32', 'gdi32')
    add_deps('lc-backends-dummy', {
        inherit = false,
        links = false
    })
    add_files('src/**.cpp')
end

interface_target('rbc_display_plugin', rbc_display_interface, rbc_display_impl, true)
