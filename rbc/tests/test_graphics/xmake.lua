local function test_graphics_interface()
    add_includedirs('generated', {
        interface = true
    })
end

local function test_graphics_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    add_defines('STANDALONE')
    add_files('**.cpp')
    add_deps('rbc_runtime', 'lc-gui', 'rbc_render_interface', 'rbc_ipc')
    add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_EXPORT')
end

interface_target('test_graphics', test_graphics_interface, test_graphics_impl)
do
    add_defines('TEST_GRAPHICS_API=LUISA_DECLSPEC_DLL_IMPORT', {public = true})
    -- STANDALONE
    add_deps('rbc_render_plugin', 'lc-backends-dummy', {inherit = false})
end