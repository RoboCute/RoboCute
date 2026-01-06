target("RBCE_ConnectionPlugin")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickWidgets", "QtNetwork")
    add_files("plugin_resource.qrc")
    add_files("*.h") -- MOC will automatically process headers with Q_OBJECT
    add_files("*.cpp")
    add_headerfiles("*.h")
    add_deps("rbc_editor_runtimex")
end
target_end()
