target("rbc_editor")
    add_rules('lc_basic_settings', {
        project_kind = 'binary',
        enable_exception = true
    })
    add_rules("qt.mywidget")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtNetwork")

    add_includedirs("include")
    add_headerfiles("include/RBCEditor/**.h")
    add_files("include/RBCEditor/**.h") -- qt moc file required
    add_files("src/**.cpp")
    set_pcxxheader('src/zz_pch.h')
    add_files("rbc_editor.qrc") -- qt resource files

    add_defines("LUISA_QT_SAMPLE_ENABLE_DX", "LUISA_QT_SAMPLE_ENABLE_VK")

    -- Dependencies
    add_deps("rbc_core")  -- For json_serde (yyjson)
    add_deps("rbc_world")
    add_deps('rbc_runtime', 'lc-gui', 'rbc_render_interface')
    add_deps("lc-volk")
    add_deps("qt_node_editor")
target_end()