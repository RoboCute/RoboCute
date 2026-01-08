target("rbc_editor_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick")
    add_deps("rbc_editor_runtimex", {
        public = true
    })
    add_files("rbc_editor_module.cpp") -- module entry
    add_deps("argparse")
    -- plugins 
end
target_end()

target("rbc_testbed_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickControls2", "QtNetwork")

    add_files("rbc_testbed_module.cpp") -- module entry
    add_deps("rbc_editor_runtimex", {
        public = true
    })
    add_deps("argparse")
end
target_end()

target('rbc_editorx')
do
    add_rules("lc_basic_settings", {
        project_kind = "binary"
    })
    set_group("04.targets")

    add_deps('rbc_editor_module', 'rbc_testbed_module', 'rbc_runtime')
    add_files('main_entry.cpp')
end
target_end()
