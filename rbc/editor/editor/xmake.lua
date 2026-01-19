target("rbc_editor_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_rules('rbc_qt_rule')
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick")
    add_deps("rbc_editor_runtime", {
        public = true
    })
    add_files("rbc_editor_module.cpp") -- module entry
end
target_end()

target("rbc_editor_test_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_rules('rbc_qt_rule')
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickControls2", "QtNetwork")

    add_files("rbc_editor_test_module.cpp") -- module entry
    add_deps("rbc_editor_runtime", {
        public = true
    })
end
target_end()

target('rbc_editor')
do
    add_rules("lc_basic_settings", {
        project_kind = "binary"
    })
    set_group("04.targets")

    add_deps('rbc_runtime')
    add_files('main_entry.cpp')
end
target_end()
