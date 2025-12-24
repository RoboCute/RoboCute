includes('test_py_codegen')
includes('test_serde', 'test_graphics', 'test_ipc')
includes("test_resource")
includes("test_coro")
includes("sample_anim")

function add_test(name, deps)
    deps = deps or {}
    target("test_" .. name)
        set_kind("binary")
        set_group("02.tests")
        on_load(function (target)
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
        add_includedirs("_framework")
        add_files("_framework/test_util.cpp")
    target_end()
end

add_test("core", { "rbc_core" })
add_test("world", { "rbc_runtime", "rbc_core" })
add_test("anim", { "rbc_runtime", "rbc_core" })


-- 一些第三方库的测试用例，用来检测第三方库是否稳定
includes("calculator") -- for qt_node_editor
