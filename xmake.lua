set_xmakever("3.0.6")
add_rules("mode.release", "mode.debug", "mode.releasedbg")
set_policy("build.ccache", not is_plat("windows"))
set_languages("c++20")
set_policy("check.auto_ignore_flags", false)
set_policy("build.optimization.lto", false)
includes('xmake/options.lua')

lc_options = {
    lc_cpu_backend = false,
    lc_cuda_backend = true,
    lc_dx_backend = is_host("windows"),
    lc_vk_backend = true,
    lc_enable_mimalloc = true,
    lc_enable_api = false,
    lc_enable_dsl = true,
    lc_enable_gui = true,
    lc_enable_imgui = false,
    lc_enable_osl = false,
    lc_enable_ir = false,
    lc_rtti = true,
    lc_enable_tests = false,
    lc_sdk_dir = "",
    -- lc_win_runtime = "MD",
    lc_metal_backend = is_host("macosx"),
    lc_dx_cuda_interop = true,
    lc_vk_cuda_interop = true,
    lc_enable_py = false,
    lc_enable_unity_build = true,
    lc_warnings = 'all',
    lc_safe_mode = true
    -- lc_toy_c_backend = true
}

includes('xmake/py_codegen.lua', 'xmake/option_meta.lua', 'xmake/interface_target.lua')
if has_config('rbc_editor') then
    includes('xmake/qt/rbc_rules.lua') -- our special compilation rules for Qt
end
includes("thirdparty", "rbc")

-- Suppress common third-party warnings for MSVC (cl.exe)
add_cxflags("/wd4090", "/wd4102", "/wd4146", "/wd4200", "/wd4267", "/wd4307", "/wd4819", "/wd4996", "/wd4018",
    "/wd4333", "/wd4172", "/wd4100", {
        tools = "cl"
    })
-- Suppress third-party warnings for Clang
add_cxflags("-Wno-deprecated-literal-operator", "-Wno-microsoft-include", "-Wno-invalid-offsetof",
    "-Wno-pragma-system-header-outside-header", "-Wno-macro-redefined", "-Wno-deprecated-declarations",
    "-Wno-unused-function", "-Wno-format", "-Wno-nullability", "-Wno-delete-non-abstract-non-virtual-dtor",
    "-Wno-duplicate-decl-specifier", "-Wno-uninitialized", "-Wno-reorder-ctor", "-Wno-unused-lambda-capture",
    "-Wno-switch", "-Wno-mismatched-tags", "-Wno-incompatible-pointer-types-discards-qualifiers", {
        tools = "clang_cl"
    })
