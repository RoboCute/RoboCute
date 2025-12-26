target('ozz_animation_include')
    set_kind('phony')
    add_includedirs("ozz_animation/include", { public = true})
target_end()

target("ozz_animation_base")
    set_kind("static")
    add_headerfiles("ozz_animation/include/**.h")
    add_files("ozz_animation/src/base/**.cc")
    add_deps('ozz_animation_include')
target_end()

target("ozz_animation_runtime_static")
    set_kind("static")
    add_deps("ozz_animation_base")
    add_includedirs("ozz_animation/src", { private = true})
    add_files("ozz_animation/src/animation/runtime/*.cc")
    add_files("ozz_animation/src/geometry/**.cc")
    add_files("ozz_animation/src/options/**.cc")
    add_defines("OZZ_BUILD_ANIMATION_LIB")
target_end()

target("ozz_json_cpp")
    set_kind("shared")
    add_files("ozz_animation/extern/jsoncpp/dist/jsoncpp.cpp")
    add_includedirs("ozz_animation/extern/jsoncpp/dist/", { public = true })
    add_defines("JSON_DLL_BUILD", { private = true });
    add_defines("JSON_DLL", { public = true })
target_end()


target("ozz_animation_offline_static")
    set_kind("static")
    add_deps("ozz_animation_base")
    add_includedirs("ozz_animation/src", { private = true})
    -- jsoncpp
    add_deps("ozz_json_cpp", { private = true })
    -- general offline animation tools
    add_files("ozz_animation/src/animation/offline/*.cc")
    add_files("ozz_animation/src/animation/offline/tools/*.cc")
    add_includedirs("ozz_animation/src/animation/offline/tools/", { private = true})
target_end()