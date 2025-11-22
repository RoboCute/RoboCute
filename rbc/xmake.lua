function tool_target(name)
    local bin_name = name .. '_b'
    target(name)
    set_kind('phony')
    add_rules('lc_run_target')
    add_deps(bin_name, 'lc-backends-dummy', 'rbc_render_plugin', {
        inherit = false
    })
    target_end()

    target(bin_name)
    set_basename(name)
end


includes('core', 'runtime')
includes('render_plugin')
includes('tests')
includes('tools.lua')
includes('editor')

includes('samples')
