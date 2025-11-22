target("editor")
    set_kind("binary")
    add_rules("qt.widgetapp")
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

