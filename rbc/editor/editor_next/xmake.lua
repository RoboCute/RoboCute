target("rbc_editor_next")

add_rules('lc_basic_settings', {
    project_kind = 'binary',
    enable_exception = true,
    rtti = true
})

add_rules("qt.console")

add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickControls2", "QtNetwork")

add_includedirs("..", {
    public = true
})

add_files("main.cpp")

-- Dependencies
add_deps("rbc_editor_runtime_next")

target_end()

