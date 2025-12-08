local function rbc_app_interface()
    add_includedirs('include', {
        public = true
    })
    add_deps('rbc_runtime', 'rbc_core', 'rbc_render_plugin')
end

local function rbc_app_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    set_pcxxheader('src/zz_pch.h')
    add_files('src/**.cpp')
    add_deps('lc-gui')
    add_defines('RBC_APP_API=LUISA_DECLSPEC_DLL_EXPORT')
end

interface_target('rbc_app', rbc_app_interface, rbc_app_impl)
add_defines('RBC_APP_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})