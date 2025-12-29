local function rbc_runtime_interface()
    add_includedirs('include', {
        public = true
    })
end

local function rbc_runtime_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_defines('RBC_NODE_API=LUISA_DECLSPEC_DLL_EXPORT')
    add_deps('rbc_runtime')
    set_pcxxheader('src/zz_pch.h')
    add_files('src/**.cpp')
end
interface_target('rbc_node', rbc_runtime_interface, rbc_runtime_impl)
add_defines('RBC_NODE_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
