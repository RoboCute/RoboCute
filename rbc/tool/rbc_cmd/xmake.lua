-- The 'rbc' commandline tool
target("rbc")
add_rules('lc_basic_settings', {
    project_kind = 'binary',
    enable_exception = true,
    rtti = true
})
add_rules("qt.console")
set_group("04.targets")

add_frameworks("QtCore", "QtNetwork", "QtGui")

add_files("main.cpp")

add_deps("rbc_runtime")
add_deps("rbc_project_plugin")

add_deps("argparse")

target_end()
