target("RBCTracy")
    add_rules("lc_basic_settings", {
        project_kind = "static"
    })
    add_headerfiles("tracy/tracy/*.h")
    add_includedirs("tracy/tracy/", { public = true})
    add_files("tracy/tracy/TracyClient.cpp")
    add_defines("TRACY_EXPORTS", { private = true })
    add_defines("TRACY_IMPORTS", { interface = true })
    add_defines("RBC_PROFILE_ENABLE", { public = true })
    -- 确保 TRACY_ENABLE 在编译 TracyClient.cpp 时被定义
    add_defines("TRACY_ENABLE", { private = true })
target_end()