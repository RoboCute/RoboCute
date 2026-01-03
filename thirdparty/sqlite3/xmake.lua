target('sqlite3')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_files('*.c')
add_includedirs('.', {
    interface = true
})
on_load(function(target)
    if target:is_plat('windows') then
        target:add('defines', 'SQLITE_API=__declspec(dllexport)')
        target:add('defines', 'SQLITE_API=__declspec(dllimport)', {
            interface = true
        })
    else
        target:add('defines', 'SQLITE_API=__attribute__((visibility("default")))')
    end
end)
target_end()
