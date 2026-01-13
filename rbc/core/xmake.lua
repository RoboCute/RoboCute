local function rbc_core_interface()
    add_includedirs('include', {
        public = true
    })
    add_deps('lc-core', 'lc-vstl', 'lc-yyjson')
    add_interface_depend('RBCTracy')
end

local function rbc_core_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    rbc_unity_build(4)
    rbc_set_pch('src/zz_pch.h')
    add_files('src/**.cpp')
    add_deps('rtm', 'RBCTracy')
    add_defines('RBC_CORE_API=LUISA_DECLSPEC_DLL_EXPORT')
end

interface_target('rbc_core', rbc_core_interface, rbc_core_impl)
add_defines('RBC_CORE_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
