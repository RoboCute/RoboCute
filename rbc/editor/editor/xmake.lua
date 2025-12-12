target("rbc_editor")
add_rules('lc_basic_settings', {
    project_kind = 'binary',
    enable_exception = true,
    rtti = true
})

add_rules("qt.widgetapp")
add_deps("rbc_editor_runtime")
add_files("main.cpp")
add_defines('RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT')

target_end()
