target("Jolt")
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_includedirs("jolt_physics/", { public = true})
    add_files("jolt_physics/Jolt/**.cpp")
    add_defines("JPH_SHARED_LIBRARY", { public = true })
    add_defines("JPH_BUILD_SHARED_LIBRARY")
target_end()
