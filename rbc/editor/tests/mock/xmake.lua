local function rbc_editor_mock_interface()
    add_includedirs("include", {
        public = true
    })
    add_deps("rbc_editor_runtime", {
        public = true
    })
end

local function rbc_editor_mock_impl()
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        rtti = true
    })
    add_rules("qt.rbc_shared")
    add_frameworks("QtCore", "QtGui", "QtWidgets", "QtQml", "QtQuick", "QtQuickWidgets", "QtQuickControls2", "QtNetwork")
    add_files("src/**.cpp")
    add_files("include/RBCEditorMock/**.h") -- qt moc
    -- add_files("rbc_mock.qrc") -- mock source file
    add_headerfiles("include/RBCEditorMock/**.h")
    add_defines("RBC_EDITOR_MOCK_API=LUISA_DECLSPEC_DLL_EXPORT")
end

interface_target('rbc_editor_mock', rbc_editor_mock_interface, rbc_editor_mock_impl)
add_defines('RBC_EDITOR_MOCK_API=LUISA_DECLSPEC_DLL_IMPORT', {
    public = true
})
