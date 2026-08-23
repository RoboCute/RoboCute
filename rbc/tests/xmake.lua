-- Boost.UT based test suite for RBC
-- Mimics thirdparty/LuisaCompute/src/tests/xmake.lua style:
-- one target per test group, header-only ut.hpp under rbc/tests/ut.

local function add_rbc_ut(name, files, deps, interface_deps)
    deps = deps or {}
    interface_deps = interface_deps or {}
    target(name)
        add_rules('lc_basic_settings', {
            project_kind = 'binary',
            enable_exception = true
        })
        set_group("02.tests")
        add_includedirs("./_framework")
        add_files("_framework/test_main.cpp")

        for _, f in ipairs(files) do
            add_files(f)
        end
        for _, dep in ipairs(deps) do
            add_deps(dep)
        end
        for _, dep in ipairs(interface_deps) do
            add_interface_depend(dep)
        end
    target_end()
end

-- show how to use test framework
add_rbc_ut("ut_test", {
    "_test/*.cpp"
}, {"rbc_core"})

-- basic dependencies
add_rbc_ut("ut_basic", {
    "basic/*.cpp"
}, {"rbc_core"})


-- core & data-structure tests


-- add_rbc_ut_test("test_core", {
--     "core/*.cpp"
-- }, {"rbc_core"})

-- runtime / animation
-- add_rbc_ut_test("test_anim", {
--     "anim/*.cpp"
-- }, {"rbc_runtime"})

-- rendering utilities
-- add_rbc_ut_test("test_render", {
--     "render/*.cpp"
-- }, {"rbc_core"}, {"rbc_render_plugin"})

-- shader runtime
-- add_rbc_ut_test("test_shader_runtime", {
--     "shader_runtime/*.cpp"
-- }, {"rbc_runtime"})

-- model importer tests
-- add_rbc_ut_test("test_model", {
--     "test_model/*.cpp"
-- }, {"rbc_runtime"}, {"rbc_importer_plugin"})

-- URDF tests
-- if has_config('rbc_urdf') then
--     add_rbc_ut_test("test_urdf", {
--         "test_urdf/*.cpp"
--     }, {"urdfdom_model"})
-- end
