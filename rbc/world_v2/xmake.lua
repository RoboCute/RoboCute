local function rbc_world_interface()
    add_includedirs('include', {
        public = true
    })
end
local function rbc_world_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    set_pcxxheader('src/zz_pch.h')
    add_deps('rbc_runtime')
    add_deps('tinyexr', 'tiny_obj_loader', 'stb-image', 'open_fbx', 'tinytiff') -- thirdparty
    add_deps('rbc_ext_c_int__')
    add_files('src/**.cpp')
    add_defines('RBC_WORLD_API=LUISA_DECLSPEC_DLL_EXPORT')
end

interface_target('rbc_world_v2', rbc_world_interface, rbc_world_impl)
add_defines('RBC_WORLD_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
