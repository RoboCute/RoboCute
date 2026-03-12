target('ozz_animation_include')
do
    set_kind('phony')
    add_includedirs("ozz_animation/include", {
        public = true
    })
end
target_end()

target("ozz_animation_base")
do
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    add_headerfiles("ozz_animation/include/**.h")
    add_deps('ozz_animation_include')

    add_includedirs("ozz_animation/extern/jsoncpp/dist/", {
        public = true
    })
    add_includedirs("ozz_animation/src", "ozz_animation/src/animation/offline/tools/")
    
    add_files("ozz_animation/src/base/**.cc", "ozz_animation/src/animation/runtime/*.cc",
        "ozz_animation/src/geometry/**.cc", "ozz_animation/src/options/**.cc",
        "ozz_animation/src/animation/offline/*.cc", "ozz_animation/src/animation/offline/tools/*.cc")
    add_files("ozz_animation/extern/jsoncpp/dist/jsoncpp.cpp")

    add_defines("OZZ_BUILD_ANIMATION_LIB", "OZZ_BUILD_OPTIONS_LIB", "OZZ_BUILD_BASE_LIB", "OZZ_BUILD_ANIMOFFLINE_LIB",
        "OZZ_BUILD_ANIMATIONTOOLS_LIB")
    add_defines('OZZ_USE_DYNAMIC_LINKING', {
        public = true
    })
    -- add_defines("JSON_DLL_BUILD");
    -- add_defines("JSON_DLL", {
    --     public = true
    -- })

end
target_end()

-- target("ozz_animation_runtime_static")
--     add_rules('lc_basic_settings', {
--         project_kind = 'static'
--     })
--     add_deps("ozz_animation_base")
--     add_includedirs("ozz_animation/src", { private = true})
--     add_files("ozz_animation/src/animation/runtime/*.cc")
--     add_files("ozz_animation/src/geometry/**.cc")
--     add_files("ozz_animation/src/options/**.cc")
--     add_defines("OZZ_BUILD_ANIMATION_LIB")
--     add_defines('OZZ_USE_DYNAMIC_LINKING', {interface = true})
-- target_end()

-- target("ozz_json_cpp")
--     add_rules('lc_basic_settings', {
--         project_kind = 'shared',
--         enable_exception = true
--     })

-- target_end()

-- target("ozz_animation_offline_static")
--     add_rules('lc_basic_settings', {
--         project_kind = 'static'
--     })
--     add_deps("ozz_animation_base")
--     add_includedirs("ozz_animation/src", { private = true})
--     -- jsoncpp
--     add_deps("ozz_json_cpp", { private = true })
--     -- general offline animation tools

-- target_end()
