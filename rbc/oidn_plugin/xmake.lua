local function oidn_plugin_interface()
    add_includedirs('include', {
        public = true
    })
    add_deps('lc-runtime')
end

local function oidn_plugin_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared' -- this is a plugin, it must be shared
    })
    add_files('src/**.cpp')
    add_deps('oidn_interface')
    on_load(function(target)
        target:add('links', 'OpenImageDenoise', 'OpenImageDenoise_core')
    end)
    add_deps('lc-vstl')
end
interface_target('oidn_plugin', oidn_plugin_interface, oidn_plugin_impl, true)

-- add_deps('dxcuda-interop')

target_end()

target('oidn_checker')
do
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    on_load(function(target)
        target:add('deps', 'lc-backends-dummy', {
            inherit = false,
            links = false
        })
        target:add('deps', 'oidn_plugin', 'lc-volk')
    end)
    add_files('oidn_checker.cpp')
end
target_end()
