local function rbc_importer_interface()
    add_includedirs('include', {
        public = true
    })
end

local function rbc_importer_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    rbc_set_pch('src/zz_pch.h')
    add_deps('rbc_runtime')
    add_files('src/**.cpp')
    add_deps('tinyexr', 'tiny_obj_loader', 'stb-image', 'open_fbx', 'tinytiff', "tinygltf", "tinyxml2", "ozz_animation_base") -- thirdparty
end

interface_target('rbc_importer_plugin', rbc_importer_interface, rbc_importer_impl, true)
