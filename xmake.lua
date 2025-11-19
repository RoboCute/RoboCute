set_xmakever("3.0.4")
add_rules("mode.release", "mode.debug", "mode.releasedbg")
set_policy("build.ccache", not is_plat("windows"))
set_policy("check.auto_ignore_flags", false)

lc_options = {
    lc_cpu_backend = false,
    lc_cuda_backend = false,
    lc_dx_backend = is_host("windows"),
    lc_vk_backend = true,
    lc_enable_mimalloc = true,
    lc_enable_cuda = false,
    lc_enable_api = false,
    lc_enable_dsl = true,
    lc_enable_gui = true,
    lc_enable_osl = false,
    lc_enable_ir = false,
    lc_enable_tests = false,
    lc_backend_lto = false,
    lc_sdk_dir = "thirdparty/LuisaCompute/SDKs",
    lc_xrepo_dir = "",
    lc_win_runtime = "MD",
    lc_metal_backend = is_host("macosx"),
    lc_dx_cuda_interop = true,
    lc_vk_cuda_interop = true,
    lc_enable_py = false,
    spdlog_only_fmt = true,
    -- lc_toy_c_backend = true
}

includes('scripts/xmake/py_codegen.lua', 'scripts/xmake/option_meta.lua')
includes("thirdparty", "rbc")

target("lc-runtime")
add_defines("LUISA_ENABLE_SAFE_MODE", {
    public = true
})
target_end()