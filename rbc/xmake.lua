-- Modules
includes('core') -- 底层数据结构
includes('world') -- 场景和资源定义
includes('runtime') -- （图像，动画，物理，插件等）运行时功能
includes('ipc') -- 跨进程
includes('render_plugin')
-- Utils
includes('tools.lua')
includes('tests')
-- Targets
includes('editor')
includes('samples')
includes("ext_c") -- The Pybind Extension rbc_ext_c.pyd