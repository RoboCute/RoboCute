target("calculator_module")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtNetwork")
    add_files("main.cpp", "MathOperationDataModel.cpp", "NumberDisplayDataModel.cpp", "NumberSourceDataModel.cpp")
    add_files("*.hpp")
    add_headerfiles("*.hpp")
    add_deps("qt_node_editor", 'rbc_core')
    add_defines("RBC_TEST_QT_MODULE")
end
target_end()

target('calculator')
do
    add_rules("lc_basic_settings", {
        project_kind = "binary"
    })
    add_deps('calculator_module', 'rbc_runtime')
    add_files('main_entry.cpp')
end
target_end()
