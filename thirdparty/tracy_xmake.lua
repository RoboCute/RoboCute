-- version 0.13.1
local function tracy_interface()
    add_includedirs("tracy", {
        public = true
    })
    add_defines("RBC_PROFILE_ENABLE", {
        public = true
    })
end

local function tracy_impl()
    add_headerfiles("tracy/tracy/*.h")
    add_rules("lc_basic_settings", {
        project_kind = "static"
    })
    add_files("tracy/tracy/TracyClient.cpp")

    -- TRACY_EXPORTS 只在编译静态库本身时定义，不使用 TRACY_IMPORTS
    -- 因为静态库不需要 DLL 导入/导出语义
    add_defines("TRACY_EXPORTS")
    -- TRACY_ENABLE 必须在所有使用 Tracy 的代码中都定义
    -- 虽然 tracy_wrapper.h 会在 RBC_PROFILE_ENABLE 定义时自动定义 TRACY_ENABLE，
    -- 但为了确保在所有情况下都能正确启用，我们也显式地通过 interface 传播
    add_defines("TRACY_ENABLE") -- 编译 TracyClient.cpp 时
    -- 注意：TRACY_ENABLE 会通过 tracy_wrapper.h 自动定义（当 RBC_PROFILE_ENABLE 存在时）
    -- 所以不需要显式设为 public，但确保 RBC_PROFILE_ENABLE 被正确传播即可
end
interface_target('RBCTracy', tracy_interface, tracy_impl)