local function oidn_plugin_interface()
    add_includedirs('include', {
        public = true
    })
end

local function oidn_plugin_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared' -- this is a plugin, it must be shared
    })
    add_files('src/**.cpp')
    add_deps('oidn_interface', 'install_oidn')
    on_load(function(target)
        target:add('links', 'OpenImageDenoise', 'OpenImageDenoise_core')
    end)
    add_deps('lc-vstl', 'lc-runtime', 'rbc_runtime')
end
interface_target('oidn_plugin', oidn_plugin_interface, oidn_plugin_impl, true)
