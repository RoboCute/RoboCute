-- Test Framework for RBC Runtime
includes('test_graphics')
includes('test_project')
includes('test_model')
includes('test_skeleton')
includes('test_anim_sequence')
if has_config('rbc_urdf') then
    includes('test_urdf')
end
-- has_config('rbc_tools') then
-- includes("test_coro")
-- includes("test_sql")
-- end

includes("sample_anim")

function add_test(name, deps, interface_deps)
    deps = deps or {}
    interface_deps = interface_deps or {}
    target("test_" .. name)
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
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
    for _, dep in ipairs(interface_deps) do
        add_interface_depend(dep)
    end
    add_includedirs("_framework")
    add_files("_framework/test_util.cpp")
    target_end()
end

add_test("core", {"rbc_core"})
add_test("anim", {"rbc_runtime", "rbc_core"})
add_test("shader_runtime", {"rbc_runtime", "rbc_core"})
add_test("render", {"rbc_core"}, {"rbc_render_plugin"})

-- includes('agents')
