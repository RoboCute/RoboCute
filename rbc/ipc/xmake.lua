local function rbc_ipc_interface()
    add_includedirs("include", {
        public = true,
        enable_exception = true
    })
end
local function rbc_ipc_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_deps('cpp-ipc', 'rbc_core')
    rbc_set_pch('src/zz_pch.h')
    add_files("src/**.cpp")
    add_defines('RBC_IPC_API=LUISA_DECLSPEC_DLL_EXPORT')
end

interface_target('rbc_ipc', rbc_ipc_interface, rbc_ipc_impl)
add_defines('RBC_IPC_API=LUISA_DECLSPEC_DLL_IMPORT', {public = true})
