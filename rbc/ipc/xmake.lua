local function rbc_ipc_interface()
    add_includedirs("include", {
        public = true
    })
    add_links('cpp-ipc', {
        interface = true
    })
end
local function rbc_ipc_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_deps('cpp-ipc', 'lc-core', 'lc-vstl')
    set_pcxxheader('src/zz_pch.h')
    add_links('cpp-ipc', {
        interface = true
    })
    add_files("src/**.cpp")
end

interface_target('rbc_ipc', rbc_ipc_interface, rbc_ipc_impl)
