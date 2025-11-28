includes("LuisaCompute")
includes("rtm")
includes("tiny_obj_loader")
includes("tinyexr")
includes("tinytiff")
includes("open_fbx")
includes("cpp-ipc")


target("ozz_animation_base")
    set_kind("static")
    add_headerfiles("ozz_animation/include/**.h")
    add_includedirs("ozz_animation/include", { public = true})
    add_files("ozz_animation/src/base/**.cc")
target_end()

target("ozz_animation_runtime")
    set_kind("shared")
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


target("qt_node_editor")
    set_kind("shared")
    add_rules("qt.rbc_shared")
    add_frameworks("QtGui", "QtWidgets", "QtCore", "QtOpenGL")
    add_headerfiles("qt_node_editor/include/QtNodes/internal/*.hpp")
    add_files("qt_node_editor/include/QtNodes/internal/*.hpp")
    add_files("qt_node_editor/resources/resources.qrc")
    add_files("qt_node_editor/src/*.cpp") -- source file
    add_includedirs("qt_node_editor/include", { public = true})
    add_includedirs("qt_node_editor/src", "qt_node_editor/include/QtNodes/internal", { private = true})
    add_defines("NODE_EDITOR_SHARED", { public = true})
    add_defines("NODE_EDITOR_EXPORTS", "QT_NO_KEYWORDS", { private = true})
target_end()
