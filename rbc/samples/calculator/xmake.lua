target("calculator")
    add_rules("lc_basic_settings", {
        project_kind = "binary",
        rtti = true
    })
    add_rules("qt.mywidget")
    add_files(
        "main.cpp",
        "MathOperationDataModel.cpp",
        "NumberDisplayDataModel.cpp",
        "NumberSourceDataModel.cpp"
    )
    add_files("*.hpp")
    add_headerfiles("*.hpp")
    add_deps("qt_node_editor")
target_end()