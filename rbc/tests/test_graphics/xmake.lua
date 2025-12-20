target('test_graphics_scene')
set_kind('phony')
add_rules('lc_install_sdk', {
    sdk_dir = 'build/download',
    libnames = {
        name = 'test_scene_v1.0.1.7z',
        address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/',
    }
})
target_end()

local targets = {
    test_graphics = false,
    test_graphics_bin = true
}
for target_name, is_standalone in pairs(targets) do
    local function test_graphics_interface()
        add_includedirs('generated', {
            interface = true
        })
        add_deps('rbc_app')
    end

    local function test_graphics_impl()
        add_rules('lc_basic_settings', {})
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
            target:add('deps', 'rbc_render_plugin', 'rbc_ipc', 'lc-gui', 'rbc_world_v2', 'compile_shaders')
            target:add('deps', 'test_graphics_scene', {inherit = false})
            target:add('defines', 'TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_EXPORT')
        end)
    end

    interface_target(target_name, test_graphics_interface, test_graphics_impl)
    add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_IMPORT', {
        public = true
    })
end
