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
        add_files('**.cpp')
        add_deps('lc-gui', 'rbc_render_interface', 'rbc_ipc')
        on_load(function(target)
            if is_standalone then
                target:set('kind', 'binary')
                target:add('defines', 'STANDALONE')
            else
                target:set('kind', 'shared')
            end
        end)
        add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_EXPORT')
    end

    interface_target(target_name, test_graphics_interface, test_graphics_impl)
    do
        add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_IMPORT', {
            public = true
        })
        on_load(function(target)
            if is_standalone then
                target:add('deps', 'rbc_render_plugin', 'lc-backends-dummy', {
                    inherit = false
                })
            end
        end)
    end

end
