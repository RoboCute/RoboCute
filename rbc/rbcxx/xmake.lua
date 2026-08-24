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
add_deps("lc-backends-dummy", {
    inherit = false,
    links = false
})
after_build(function(target)
    -- TODO: macos and linux
    if not target:is_plat("windows") then
        return
    end
    local dst_dir = path.join(os.projectdir(), "build/tool/rbcxx")
    os.mkdir(dst_dir)
    local files = {"rbcxx.exe", "dxcompiler.dll", "dxil.dll", "rbc-lc-clangcxx.dll", "luisa-core.dll",
                   "luisa-runtime.dll"}
    if has_config("lc_vk_backend") then
        table.insert(files, "luisa-backend-vk.dll")
    end
    if has_config("lc_dx_backend") then
        table.insert(files, "luisa-backend-dx.dll")
    end
    for i, v in ipairs(files) do
        os.cp(path.join(target:targetdir(), v), dst_dir, {
            copy_if_different = true,
            async = true,
            detach = true
        })
    end
    os.cp(path.join(os.scriptdir(), "template.txt"), dst_dir, {
        copy_if_different = true,
        async = true,
        detach = true
    })
end)
target_end()
end
