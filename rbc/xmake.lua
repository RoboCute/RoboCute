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

-- Modules
includes('core') -- 底层数据结构
includes('world') -- 场景和资源定义
includes('runtime') -- （图像，动画，物理，插件等）运行时功能
includes('render_plugin')
-- Utils
includes('tools.lua')
includes('tests')
-- Targets
includes('editor')
includes('samples')
includes("ext") -- The Pybind Extension rbc_ext