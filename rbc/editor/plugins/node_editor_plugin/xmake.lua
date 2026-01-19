target("RBCE_NodeEditorPlugin")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    set_group("RBCEditorPlugins")
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickWidgets", "QtNetwork")
    add_files("*.h") -- MOC will automatically process headers with Q_OBJECT
    add_files("*.cpp")
    add_headerfiles("*.h")
    add_deps("rbc_editor_runtimex")
    add_rules('rbc_qt_rule')
    add_defines("RBC_EDITOR_PLUGIN_API=LUISA_DECLSPEC_DLL_EXPORT")
end

target_end()
