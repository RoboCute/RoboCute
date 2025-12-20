includes('test_py_codegen')
includes('test_serde', 'test_graphics', 'test_ipc')


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
