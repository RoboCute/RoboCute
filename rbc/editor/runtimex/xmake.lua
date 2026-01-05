target("rbc_editor_runtimex")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickWidgets", "QtQuickControls2", "QtNetwork")

    add_files("src/**.cpp")
    add_files("include/RBCEditorRuntime/**.h")
    add_files("rbc_editor.qrc") -- source file
    add_headerfiles("include/RBCEditorRuntime/**.h")
    add_includedirs("include", {
        public = true
    })
    add_deps("qt_node_editor", 'rbc_core')
    add_defines('RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT')
    add_defines('RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT', {
        interface = true
    })
end

target_end()
