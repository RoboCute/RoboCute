target('rbc_runtime')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
add_includedirs('include', '../shader/include', {
    public = true
})
add_files('src/**.cpp')
set_pcxxheader('src/zz_pch.h')
add_deps('rbc_world', 'lc-runtime', 'lc-volk')
add_defines('RBC_RUNTIME_EXPORT_DLL')
on_load(function(target)
    if target:is_plat('windows') then
        target:add("syslinks", "d3d12", "User32")
    end
end)
target_end()
