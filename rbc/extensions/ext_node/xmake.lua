-- rbc_ext_node: N-API addon (Node.js/Electron) — spike counterpart of rbc_ext_c.
-- Requires `pnpm install` in samples/electron first (provides node-api-headers).
local node_headers = path.join(os.projectdir(), 'samples/electron/node_modules/node-api-headers/include')
if not os.isdir(node_headers) then
    print("[rbc_ext_node] node-api-headers not found, skipping target. Run `pnpm install` in samples/electron.")
    return
end

target("rbc_ext_node")
do
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true
    })
    add_deps('rbc_core', 'sample_graphics', 'rbc_render_plugin')
    set_extension('.node')
    add_includedirs(node_headers)
    -- N-API import library (downloaded from the node binary mirror, see
    -- samples/electron/native/deps). N-API is ABI-stable, so this links fine
    -- for both Node.js and Electron.
    add_linkdirs(path.join(os.projectdir(), 'samples/electron/native/deps'))
    add_links("node", "delayimp", "dbghelp", "user32")
    -- Delay-load node.exe so the hook in win_delay_load_hook.cpp can redirect
    -- it to the host process (required inside Electron, which has no node.exe).
    add_ldflags("/delayload:node.exe", {tools = {"link", "clang_cl", "lld_link"}})
    add_shflags("/delayload:node.exe", {tools = {"link", "clang_cl", "lld_link"}})
    add_files("src/*.cpp", "src/platform/*.cpp")
    -- the real cocoa impl will be Objective-C++; include it when it lands
    if is_plat("macosx") then
        add_files("src/platform/cocoa_viewport.mm")
    end
    -- place the .node next to the electron demo for a stable require() path
    set_targetdir(path.join(os.projectdir(), 'samples/electron/native/build'))
    set_objectdir(path.join(os.projectdir(), 'build/.objs/rbc_ext_node'))
end
target_end()
