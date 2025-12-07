local targets = {
    test_graphics = false,
    test_graphics_bin = true
}
for target_name, is_standalone in pairs(targets) do
    local function test_graphics_interface()
        add_includedirs('generated', {
            interface = true
        })
        add_deps('rbc_runtime')
    end

    local function test_graphics_impl()
        add_rules('lc_basic_settings', {})
        add_deps('lc-gui', 'rbc_render_interface', 'rbc_ipc')
        on_load(function(target)
            if is_standalone then
                for _, v in ipairs(os.files(path.join(os.scriptdir(), '**.cpp'))) do
                    if path.basename(v) ~= 'interface' then
                        target:add('files', v)
                    end
                end
                target:set('kind', 'binary')
                target:add('defines', 'STANDALONE')
            else
                for _, v in ipairs(os.files(path.join(os.scriptdir(), '**.cpp'))) do
                    if path.basename(v) ~= 'main' then
                        target:add('files', v)
                    end
                end
                target:set('kind', 'shared')
            end
            target:add('deps', 'rbc_render_plugin', 'lc-backends-dummy', {
                inherit = false,
                links = false
            })
        end)
        add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_EXPORT')
    end

    interface_target(target_name, test_graphics_interface, test_graphics_impl)
    do
        add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_IMPORT', {
            public = true
        })
    end

end
