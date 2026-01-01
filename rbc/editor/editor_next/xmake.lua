target("rbc_editor_next")

add_rules('lc_basic_settings', {
    project_kind = 'binary',
    enable_exception = true,
    rtti = true
})

add_rules("qt.quickapp")

add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickControls2", "QtNetwork")

add_includedirs("include", {
    public = true
})
add_headerfiles("include/RBCEditorRuntime/**.h")
add_files("include/RBCEditorRuntime/**.h") -- qt moc file required
add_files("src/**.cpp")
set_pcxxheader('src/zz_pch.h')
add_files("rbc_editor.qrc") -- qt resource files (includes QML files)
add_files("main.cpp")

add_defines("LUISA_QT_SAMPLE_ENABLE_DX", "LUISA_QT_SAMPLE_ENABLE_VK")

add_defines('RBC_EDITOR_RUNTIME_API=')

-- Dependencies
add_deps("rbc_core") -- For json_serde (yyjson)
add_deps('rbc_runtime', 'rbc_render_plugin', 'rbc_app')
add_deps("lc-volk")
add_deps("qt_node_editor")

target_end()

