target("rbc_world")
add_rules('lc_basic_settings', {
    project_kind = 'shared',
    enable_exception = true
})
add_includedirs('include', { public = true})
add_files('src/**.cpp')
set_pcxxheader('src/zz_pch.h')
add_deps('rbc_core')
add_defines('RBC_WORLD_EXPORT_DLL')

target_end()