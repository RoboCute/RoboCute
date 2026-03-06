local function test_transparent_window_interface()
    add_deps('rbc_runtime')
end

local function test_transparent_window_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    on_load(function(target)
        local ignore_files = {}
        for _, v in ipairs(os.files(path.join(os.scriptdir(), '**.cpp'))) do
            if not ignore_files[path.basename(v)] then
                target:add('files', v)
            end
        end
        target:add('deps', 'stb-image')
        target:add('deps', 'rbc_render_plugin', 'lc-gui', 'rbc_project_plugin', 'rbc_display_plugin')
        target:add('defines', 'TEST_TRANSPARENT_WINDOW_API=LUISA_DECLSPEC_DLL_EXPORT')
    end)
end

interface_target('test_transparent_window', test_transparent_window_interface, test_transparent_window_impl)
add_defines('TEST_TRANSPARENT_WINDOW_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
