local function rbc_runtime_interface()
    add_includedirs('include', '../shader/include', {
        public = true
    })
    add_deps('lc-runtime', 'rbc_core')
end

local function rbc_runtime_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_files('src/**.cpp')
    set_pcxxheader('src/zz_pch.h')
    add_deps('lc-volk', 'tinyexr', 'tiny_obj_loader', 'stb-image', 'open_fbx')
    add_defines('RBC_RUNTIME_EXPORT_DLL')
    on_load(function(target)
        if target:is_plat('windows') then
            target:add('syslinks', 'd3d12', 'User32')
        end
    end)

end

interface_target('rbc_runtime', rbc_runtime_interface, rbc_runtime_impl)
