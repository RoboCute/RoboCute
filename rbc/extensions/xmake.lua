includes("ext_c")
includes("lcapi_c")
includes("common")
-- N-API addon for the Electron demo (samples/electron). Only included when
-- the option is enabled: `uv run prepare --electron-ext` detects a valid
-- node/pnpm environment and writes rbc_ext_node=true into xmake/options.json.
-- Default is disabled, so the main build flow never touches it.
if has_config('rbc_ext_node') then
    includes("ext_node")
end
