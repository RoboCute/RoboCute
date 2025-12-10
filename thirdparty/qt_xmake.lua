target("qt_node_editor")
add_rules("qt.shared")
add_frameworks("QtGui", "QtWidgets", "QtCore", "QtOpenGL")
add_headerfiles("qt_node_editor/include/QtNodes/internal/*.hpp")
add_files("qt_node_editor/include/QtNodes/internal/*.hpp")
add_files("qt_node_editor/resources/resources.qrc")
add_files("qt_node_editor/src/*.cpp") -- source file
add_includedirs("qt_node_editor/include", {
    public = true
})
add_includedirs("qt_node_editor/src", "qt_node_editor/include/QtNodes/internal", {
    private = true
})
add_defines("NODE_EDITOR_SHARED", {
    public = true
})
add_defines("NODE_EDITOR_EXPORTS", "QT_NO_KEYWORDS", {
    private = true
})
target_end()
