-- Modules
includes('core') -- 底层数据结构

includes('runtime') -- （图像，动画，物理，插件等）运行时功能
includes('ipc') -- 跨进程
-- Render
includes('render_plugin')
includes('oidn_plugin')
-- Utils
includes('tools.lua')
includes('tests')
-- Targets
includes('editor')
-- Later need refactor from new rbc-world
-- includes("ext_c") -- The Pybind Extension rbc_ext_c.pyd
includes('node_graph')
includes('project_plugin')
