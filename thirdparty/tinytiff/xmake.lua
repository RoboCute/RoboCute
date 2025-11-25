target('tinytiff')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
on_load(function(target)
    if target:is_plat('windows') then
        target:add('defines', 'HAVE_FSEEKI64')
    end
end)
add_files('*.c')
add_includedirs(".", {
    public = true
})
target_end()

-- example
-- target("tinytiff_example")
--     add_files("example.cpp")
--     add_rules('lc_basic_settings', {
--         project_kind = 'binary'
--     })
--     add_deps("tinytiff")
-- target_end()
