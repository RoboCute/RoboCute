rule("qt.rbc_shared")
    add_deps("qt.ui", "qt.moc", "qt._wasm_app", "qt.qrc", "qt.ts")

    on_config(function (target)
        -- get qt sdk version
        local qt = target:data("qt")
        local qt_sdkver = nil
        if qt.sdkver then
            import("core.base.semver")
            qt_sdkver = semver.new(qt.sdkver)
        end
        local frameworks = {"QtGui", "QtWidgets", "QtCore", "QtOpenGL"}
        import("load")(target, {gui = true, frameworks = frameworks})
    end)
rule_end()
