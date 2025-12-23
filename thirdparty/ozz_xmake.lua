target("ozz_animation_base")
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    add_headerfiles("ozz_animation/include/**.h")
    add_includedirs("ozz_animation/include", { public = true})
    add_files("ozz_animation/src/base/**.cc")
target_end()

target("ozz_animation_static")
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    add_deps("ozz_animation_base")
    add_includedirs("ozz_animation/src", { private = true})
    add_files("ozz_animation/src/animation/runtime/*.cc")
    add_files("ozz_animation/src/geometry/**.cc")
    add_files("ozz_animation/src/options/**.cc")
    add_defines("OZZ_BUILD_ANIMATION_LIB")
target_end()

-- target("ozz_animation_offline")
--     set_kind("shared")
--     add_deps("ozz_animation_base")
--     add_includedirs("ozz_animation/src", { private = true})
--     -- general
--     add_files("ozz_animation/src/animation/offline/*.cc")
--     -- gltf
--     add_includedirs("ozz_animation/src/animation/offline/gltf", { private = true})
--     add_files("ozz_animation/src/animation/offline/gltf/*.cc")
-- target_end()