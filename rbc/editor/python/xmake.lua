target("rbc_editor_py")
do
    add_rules("lc_basic_settings", {
        project_kind = "shared",
        enable_exception = true
    })
    add_rules("qt.rbc_shared")
    add_rules("rbc_qt_rule")
    add_rules("pybind")

    set_extension(".pyd")

    -- The LC viewport lives in the editor runtime; the runtime/render plugin
    -- initialisation lives in rbc_runtime / rbc_core.
    add_deps("rbc_editor_runtime", "rbc_runtime", "rbc_core")

    add_files("src/**.cpp")
    rbc_set_pch("src/zz_pch.h")
end
target_end()
