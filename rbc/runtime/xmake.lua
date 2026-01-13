local function rbc_runtime_interface()
    add_includedirs('include', '../shader/include', {
        public = true
    })
    add_deps('lc-runtime', 'rbc_core')
    add_interface_depend('rbc_render_plugin')
end

local function rbc_runtime_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    rbc_unity_build(8)
    add_files('src/**.cpp')
    rbc_set_pch('src/zz_pch.h')
    add_deps('lc-backends-dummy', {
        inherit = false,
        links = false
    })
    add_deps('tinyexr', 'tiny_obj_loader', 'stb-image', 'open_fbx', 'tinytiff', "tinygltf") -- thirdparty
    -- third-party usage
    add_deps("ozz_animation_runtime_static", "ozz_animation_offline_static")

    -- add_interface_deps('rbc_render_plugin')
    add_deps('lc-volk')
    add_defines('RBC_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT')
    on_load(function(target)
        if target:is_plat('windows') then
            target:add('syslinks', 'd3d12', 'User32')
        end
    end)
end

interface_target('rbc_runtime', rbc_runtime_interface, rbc_runtime_impl)
add_defines('RBC_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
add_deps('ozz_animation_include')
