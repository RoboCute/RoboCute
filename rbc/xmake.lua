-- Modules
includes('tools.lua')
-- Utils
includes('core') -- 底层数据结构
includes('runtime') -- （图像，动画，物理，插件等）运行时功能
-- Plugins
includes('render_plugin')
includes('oidn_plugin')
includes('project_plugin')
if has_config('rbc_tests') then
    includes('tests')
end
-- Targets
if has_config('rbc_editor') then
    includes('editor')
    -- Later need refactor from new rbc-world
    -- includes("ext_c") -- The Pybind Extension rbc_ext_c.pyd
    includes('node_graph')
    includes("tool")
end
if has_config('rbc_tools') then
    includes('ipc') -- 跨进程
end
