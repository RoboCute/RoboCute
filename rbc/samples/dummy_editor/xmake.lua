target("dummy_editor")
    set_kind("binary")
    add_rules("qt.widgetapp")
    set_toolchains(get_config('rbc_py_toolchain'))
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtNetwork")
    add_files(
        "main.cpp",
        "EditorWindow.cpp",
        "HttpClient.cpp",
        "DynamicNodeModel.cpp",
        "NodeFactory.cpp",
        "ExecutionPanel.cpp"
    )
    add_files("*.hpp")
    add_headerfiles("*.hpp")
    add_deps("qt_node_editor")
target_end()

