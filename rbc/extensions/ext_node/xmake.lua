-- rbc_ext_node: N-API addon (Node.js/Electron) — spike counterpart of rbc_ext_c.
-- This file is only included when the `rbc_ext_node` option is enabled
-- (see rbc/extensions/xmake.lua), which `uv run prepare --electron-ext` does
-- after checking a valid node/pnpm environment and pulling the deps
-- (node-api-headers via `pnpm install`, headers tarball, tracked node.lib).
local node_headers = path.join(os.projectdir(), 'samples/electron/node_modules/node-api-headers/include')
if not os.isdir(node_headers) then
    print("[rbc_ext_node] node-api-headers not found, skipping target. Run `uv run prepare --electron-ext`.")
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
    add_links("node", "delayimp", "dbghelp")
    if is_plat("windows") then
        add_syslinks("d3d11", "dxgi")
    end
    -- Delay-load node.exe so the hook in win_delay_load_hook.cpp can redirect
    -- it to the host process (required inside Electron, which has no node.exe).
    add_ldflags("/delayload:node.exe", {tools = {"link", "clang_cl", "lld_link"}})
    add_shflags("/delayload:node.exe", {tools = {"link", "clang_cl", "lld_link"}})
    -- platform/shared_surface_*.cpp: texture export per OS (win32 D3D12 NT
    -- handle; stub elsewhere, guarded by #ifdef inside each file)
    add_files("src/*.cpp", "src/platform/*.cpp")
    -- place the .node next to the electron demo for a stable require() path
    set_targetdir(path.join(os.projectdir(), 'samples/electron/native/build'))
    set_objectdir(path.join(os.projectdir(), 'build/.objs/rbc_ext_node'))
end
target_end()
