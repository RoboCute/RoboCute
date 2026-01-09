rule("qt.rbc_shared")
add_deps("qt.ui", "qt.moc", "qt._wasm_app", "qt.qrc", "qt.ts")
on_config(function(target)
    -- get qt sdk version
    local qt = target:data("qt")
    local qt_sdkver = nil
    if qt.sdkver then
        import("core.base.semver")
        qt_sdkver = semver.new(qt.sdkver)
    end

    local frameworks = {"QtGui", "QtWidgets", "QtCore", "QtOpenGL"}
    import("load")(target, {
        gui = true,
        frameworks = frameworks
    })
    -- /Zc:__cplusplus - required for Qt 6+ on MSVC to properly report __cplusplus
    -- This flag ensures __cplusplus macro reports the correct C++ standard version
    -- Needed for both MSVC compiler and clangd (language server) when using MSVC-compatible mode
    if qt_sdkver and qt_sdkver:ge("6.0") then
        if target:is_plat("windows") then
            -- Add for MSVC compiler (cl) and clang-cl (MSVC-compatible mode)
            target:add("cxxflags", "/Zc:__cplusplus", { tools = {"cl", "clang_cl"}})
            target:add("cxxflags", "/permissive-", { tools = {"cl", "clang_cl"}})
            -- Also add for clang compiler when used with MSVC-compatible flags
            -- This ensures compile_commands.json includes the flag for clangd
            target:add("cxxflags", "/Zc:__cplusplus", { tools = {"clang"}})
        end
    elseif not qt_sdkver and target:is_plat("windows") then
        -- If Qt SDK version is not detected, assume Qt 6+ and add the flag anyway
        -- This ensures the flag is present even if Qt version detection fails
        target:add("cxxflags", "/Zc:__cplusplus", { tools = {"cl", "clang_cl", "clang"}})
        target:add("cxxflags", "/permissive-", { tools = {"cl", "clang_cl"}})
    end
    target:add('cxflags', '/Zc:__cplusplus', {
        tools = {'cl', 'clang_cl'}
    })
end)
rule_end()


rule('rbc_qt_rule')
on_load(function(target)
    target:add('cxflags', '/Zc:__cplusplus', {
        tools = 'cl'
    })
end)
rule_end()
