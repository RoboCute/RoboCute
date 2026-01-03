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
    add_rules("c++.unity_build", {
        batchsize = 4
    })
    set_pcxxheader('src/zz_pch.h')
    add_files('src/**.cpp')
    add_deps('rtm', 'sqlite3', 'RBCTracy')
    add_defines('RBC_CORE_API=LUISA_DECLSPEC_DLL_EXPORT')
end

interface_target('rbc_core', rbc_core_interface, rbc_core_impl)
add_defines('RBC_CORE_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
