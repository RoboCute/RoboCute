rule("qt.rbc_widgetapp")
    add_deps("qt.ui", "qt.moc", "qt._wasm_app", "qt.qrc", "qt.ts")

    on_config(function (target)

        -- get qt sdk version
        local qt = target:data("qt")
        local qt_sdkver = nil
        if qt.sdkver then
            import("core.base.semver")
            qt_sdkver = semver.new(qt.sdkver)
        end

        local frameworks = {"QtGui", "QtWidgets", "QtCore"}
        if qt_sdkver and qt_sdkver:lt("5.0") then
            frameworks = {"QtGui", "QtCore"} -- qt4.x has not QtWidgets, it is in QtGui
        end
        import("load")(target, {gui = true, frameworks = frameworks})
    end)

    -- deploy application
    after_build("android", "deploy.android")
    after_build("macosx", "deploy.macosx")

    -- install application for android
    on_install("android", "install.android")
    after_install("windows", "install.windows")
    after_install("mingw", "install.mingw")

    -- install application for xpack
    on_installcmd("installcmd")
    on_uninstallcmd("uninstallcmd")

