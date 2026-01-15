local function rbc_editor_runtime_interface()
    add_includedirs("include", {
        public = true
    })
    add_deps("qt_node_editor", 'rbc_core', "rbc_runtime")
end

local function rbc_editor_runtime_impl()
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.rbc_shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickWidgets", "QtQuickControls2", "QtNetwork")

    add_files("src/**.cpp")

    add_files("include/RBCEditorRuntime/**.h") -- qt moc file required

    add_files("rbc_editor.qrc") -- source file
    add_headerfiles("include/RBCEditorRuntime/**.h")

    add_defines('RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_EXPORT')
    
    -- Define RBC_SOURCE_DIR for finding layout files during development
    add_defines('RBC_SOURCE_DIR="' .. os.projectdir() .. '"')
end

interface_target('rbc_editor_runtimex', rbc_editor_runtime_interface, rbc_editor_runtime_impl)
add_defines('RBC_EDITOR_RUNTIME_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
