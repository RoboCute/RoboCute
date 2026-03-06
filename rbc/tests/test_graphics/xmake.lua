local targets = {
    test_graphics = false,
    test_graphics_bin = true
}
for target_name, is_standalone in pairs(targets) do
    local function test_graphics_interface()
        add_includedirs('include', {
            public = true
        })
        add_deps('rbc_runtime')
    end

    local function test_graphics_impl()
        add_rules('lc_basic_settings', {})
        on_load(function(target)
            if is_standalone then
                local ignore_files = {
                    rbc_context_impl = true,
                    world_impl = true,
                    mat_impl = true,
                }
                for _, v in ipairs(os.files(path.join(os.scriptdir(), '**.cpp'))) do
                    if not ignore_files[path.basename(v)]then
                        target:add('files', v)
                    end
                end
                target:set('kind', 'binary')
                target:add('defines', 'STANDALONE')
            else
                local ignore_files = {
                    main = true
                }
                for _, v in ipairs(os.files(path.join(os.scriptdir(), '**.cpp'))) do
                    if not ignore_files[path.basename(v)] then
                        target:add('files', v)
                    end
                end
                target:set('kind', 'shared')
            end
            -- target:add('deps', 'Jolt')
            target:add('deps', 'stb-image')
            target:add('deps', 'rbc_render_plugin', 'lc-gui', 'compile_shaders', 'rbc_project_plugin', 'rbc_display_plugin')
            target:add('defines', 'TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_EXPORT')
        end)
    end

    interface_target(target_name, test_graphics_interface, test_graphics_impl)
    add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_IMPORT', {
        public = true
    })
end
