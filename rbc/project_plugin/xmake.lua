local function rbc_project_interface()
    add_includedirs('include', {
        public = true
    })
end

local function rbc_project_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    set_pcxxheader('src/zz_pch.h')
    add_deps('rbc_runtime')
    add_files('src/**.cpp')
end

interface_target('rbc_project_plugin', rbc_project_interface, rbc_project_impl, true)
