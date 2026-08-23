-- Boost.UT based test suite for RBC
-- Mimics thirdparty/LuisaCompute/src/tests/xmake.lua style:
-- one target per test group, header-only ut.hpp under rbc/tests/_framework/ut.

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

-- framework example
add_rbc_ut("ut_test", {
    "_test/*.cpp"
}, {"rbc_core"})

-- basic dependencies
add_rbc_ut("ut_basic", {
    "basic/*.cpp"
}, {"rbc_core"})

-- core & data-structure tests
add_rbc_ut("ut_core", {
    "core/*.cpp"
}, {"rbc_core"})

-- animation type tests
add_rbc_ut("ut_anim", {
    "anim/*.cpp"
}, {"rbc_core", "rbc_runtime"})

-- rendering utilities
add_rbc_ut("ut_render", {
    "render/*.cpp"
}, {"rbc_core"}, {"rbc_render_plugin"})

-- shader runtime
add_rbc_ut("ut_shader_runtime", {
    "shader_runtime/*.cpp"
}, {"rbc_runtime"})

-- model importer tests
add_rbc_ut("ut_model", {
    "test_model/*.cpp"
}, {"rbc_runtime"}, {"rbc_importer_plugin"})

-- animation sequence importer tests
add_rbc_ut("ut_anim_sequence", {
    "test_anim_sequence/*.cpp"
}, {"rbc_runtime", "rbc_importer_plugin", "stb-image", "argparse"})

-- coroutine examples
add_rbc_ut("ut_coro", {
    "test_coro/*.cpp"
}, {"rbc_core"})

-- project import tests
add_rbc_ut("ut_project", {
    "test_project/*.cpp"
}, {"rbc_runtime", "lc-dsl", "rbc_project_plugin"})

-- skeleton import tests
add_rbc_ut("ut_skeleton", {
    "test_skeleton/*.cpp"
}, {"rbc_runtime", "rbc_importer_plugin", "stb-image", "argparse"})

-- sqlite tests (SqliteCpp implementation currently unavailable, so excluded from build)
-- add_rbc_ut("ut_sql", {
--     "test_sql/*.cpp"
-- }, {"rbc_core"})

-- URDF tests
if has_config('rbc_urdf') then
    add_rbc_ut("ut_urdf", {
        "test_urdf/*.cpp"
    }, {"urdfdom_model"})
end

-- animation sample (converted to ut placeholder)
add_rbc_ut("ut_sample_anim", {
    "sample_anim/*.cpp"
}, {"rbc_runtime", "rbc_importer_plugin", "lc-gui", "stb-image"})
