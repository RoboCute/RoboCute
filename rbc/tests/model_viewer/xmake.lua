target('model_viewer')
set_kind('binary')
set_group('03.samples')
add_rules('lc_basic_settings', {})
on_load(function(target)
    for k, v in pairs(opt) do
        target:set(k, v)
    end
    target:set('exceptions', 'cxx')
    target:add('deps', 'rbc_render_plugin', 'rbc_runtime', 'rbc_app', 'lc-gui', 'compile_shaders', 'RBCTracy')
end)
add_files('*.cpp')
target_end()

