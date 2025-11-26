target('cpp-ipc')
add_rules('lc_basic_settings', {
    project_kind = 'static',
    enable_exception = true
})
add_includedirs('include', {
    public = true
})
add_includedirs('src')
on_load(function(target)
    local src_path = path.join(os.scriptdir(), 'src/libipc')
    local files = {'*.cpp', 'sync/*.cpp', 'platform/platform.cpp', 'platform/platform.c'}
    for _, v in ipairs(files) do
        target:add('files', path.join(src_path, v))
    end
    if target:is_plat('windows') then
        target:add('defines', 'IPC_OS_WINDOWS_')
    elseif target:is_plat('linux') then
        target:add('defines', 'IPC_OS_LINUX_')
    end
end)
