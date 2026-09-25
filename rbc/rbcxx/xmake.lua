if not is_mode("debug") then
target("rbcxx")
_config_project({
    project_kind = "binary",
    enable_exception = true
})
on_config(function(target)
    local _, ld = target:tool("ld")
    if ld == "link" then
        target:add("ldflags", "/STACK:8388608")
    elseif ld == "gcc" or ld == "gxx" then
        target:add("ldflags", "-Wl,--stack -Wl,8388608")
    end
end)
    add_files("*.cpp")
    add_deps("rbc-lc-clangcxx", "lc-runtime", "lc-vstl", "reproc", "lc-yyjson")
    -- after_build stages luisa-backend-{dx,vk}.dll next to rbcxx.exe, so a clean
    -- parallel build must finish those targets first; otherwise os.cp of the
    -- not-yet-linked DLL fails and the whole cold build errors out.
    if has_config("lc_dx_backend") then
        add_deps("lc-backend-dx", {inherit = false})
    end
    if has_config("lc_vk_backend") then
        add_deps("lc-backend-vk", {inherit = false})
    end
after_build(function(target)
    -- TODO: macos and linux
    if not target:is_plat("windows") then
        return
    end
    local dst_dir = path.join(os.projectdir(), "build/tool/rbcxx")
    os.mkdir(dst_dir)
    local files = {"rbcxx.exe", "dxcompiler.dll", "dxil.dll", "rbc-lc-clangcxx.dll", "luisa-core.dll",
                   "luisa-runtime.dll", "luisa-tile.dll"}
    if has_config("lc_vk_backend") then
        table.insert(files, "luisa-backend-vk.dll")
    end
    if has_config("lc_dx_backend") then
        table.insert(files, "luisa-backend-dx.dll")
    end
    for i, v in ipairs(files) do
        local src = path.join(target:targetdir(), v)
        if not os.exists(src) and (v == "dxcompiler.dll" or v == "dxil.dll") then
            -- rbc_render_plugin's after_build normally stages these into the
            -- shared targetdir, but rbcxx may finish linking first on a clean
            -- parallel build; fall back to the prepared dx_sdk package.
            local dx_sdk = path.join(os.projectdir(), "build/download/dx_sdk", v)
            if os.exists(dx_sdk) then
                src = dx_sdk
            end
        end
        os.cp(src, dst_dir, {
            copy_if_different = true
        })
    end
    os.cp(path.join(os.scriptdir(), "template.txt"), dst_dir, {
        copy_if_different = true
    })
end)
target_end()
end
