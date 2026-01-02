target("rbc_editor_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtNetwork")
    add_files("src/**.cpp")
    add_files("include/EditorRuntime/**.h")
    add_headerfiles("include/EditorRuntime/**.h")
    add_deps("qt_node_editor", 'rbc_core')
    add_defines("RBC_EDITOR_MODULE")
end
target_end()

target('rbc_editorx')
add_rules("lc_basic_settings", {
    project_kind = "binary"
})
add_deps('rbc_editor_module', 'rbc_runtime')
add_files('main_entry.cpp')
target_end()
