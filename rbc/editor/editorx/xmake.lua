target("rbc_editor_runtimex")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickControls2", "QtNetwork")

    add_files("src/**.cpp")
    add_files("include/EditorRuntime/**.h")
    add_files("rbc_editor.qrc") -- source file
    add_headerfiles("include/EditorRuntime/**.h")
    add_includedirs("include", {
        public = true
    })
    add_deps("qt_node_editor", 'rbc_core')
    add_defines('RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT')
end
target_end()

target("rbc_editor_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets")
    add_deps("rbc_editor_runtimex", {
        public = true
    })
    add_files("rbc_editor_module.cpp") -- module entry
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
end
target_end()

target('rbc_editorx')
do
    add_rules("lc_basic_settings", {
        project_kind = "binary"
    })
    add_deps('rbc_editor_module', 'rbc_testbed_module', 'rbc_runtime')
    add_files('main_entry.cpp')
end
target_end()
