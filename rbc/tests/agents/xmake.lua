-- This folder is for agent to write better code, usually do not need to compile
local function test_project(name)
    target(name)
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    add_deps('external_doctest', 'rbc_core')
    add_includedirs("../_framework")
    add_files("../_framework/test_util.cpp")
    add_files(name .. ".cpp")
    target_end()
end

test_project('test_variant')
test_project('test_hashmap')
test_project('test_optional')
test_project('test_lock_free_array_queue')
