-- *** Editor tests for different layers ***
-- * Functional Layer: 一些内部实现函数的测试覆盖
-- * Service Layer: 内部服务组件的测试
-- * UI Layer: 视觉样式测试
-- * Component Layer: 小型功能组件的测试
-- * Plugin Layer: 完整插件的测试
-- * Application Layer：完整功能的闭环测试，并不会放在这里，而是在editor执行程序中直接并行嵌入
includes("calculator") -- for qt_node_editor 第三方测试库

function add_editor_test(name, deps)
    deps = deps or {}
    target("test_editor_" .. name)
    set_kind("binary")
    set_group("02.tests")
    on_load(function(target)
        for k, v in pairs(opt) do
            target:set(k, v)
        end
        target:set("exceptions", "cxx")
        for _, dep in ipairs(deps) do
            target:add("deps", dep)
        end
    end)
    add_files(name .. "/*.cpp")
    add_deps("external_doctest")
    add_deps("rbc_editor_runtimex")

    add_includedirs("_framework")
    add_files("_framework/test_util.cpp")
    target_end()
end

add_editor_test("func")
