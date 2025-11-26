target("rbc_editor")
    add_rules('lc_basic_settings', {
        project_kind = 'binary',
        enable_exception = true
    })
    set_toolchains(get_config('rbc_py_toolchain'))
    add_rules("qt.widgetapp")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtNetwork")

    add_files("*.cpp")
    add_files("*.h") -- qt moc file required
    add_files("rbc_editor.qrc") -- qt resource files
    add_headerfiles("*.h")
    -- Dependencies
    add_deps("rbc_core")  -- For json_serde (yyjson)
    add_deps("rbc_world")
    add_deps('rbc_runtime', 'lc-gui', 'rbc_render_interface')
    add_deps("qt_node_editor")
target_end()