includes("Imath_xmake.lua")
includes("LuisaCompute")

target('rbc_dllexport_include') -- to include dll_export.h
set_kind('phony')
add_includedirs('LuisaCompute/include', {
    public = true
})
target_end()

includes("rtm")
includes("tiny_obj_loader")
includes("tinyexr")
includes("tinytiff")
includes("tinyply_xmake.lua")
includes("tinyxml2_xmake.lua")
includes("open_fbx")
if has_config('rbc_tools') then
    includes("cpp-ipc")
end

-- includes('cppitertools_xmake.lua')
includes('tinygltf_xmake.lua')
-- includes('dylib_xmake.lua')
-- includes('libigl_xmake.lua')
-- includes('eigen_xmake.lua')
-- includes('cpptrace_xmake.lua')
-- includes('nlohmann_json_xmake.lua')
-- includes('libuipc')
includes("doctest")
-- includes('jolt_xmake.lua')
includes('ozz_xmake.lua')
if has_config('rbc_editor') then
    includes('qt_xmake.lua')
end
includes('oidn')
includes('tracy_xmake.lua')
-- includes('sqlite3')
includes('argparse')
includes('alembic_xmake.lua')
if has_config('rbc_urdf') then
    includes('urdfdom_xmake.lua')
end
-- target('magic_enum')
-- set_kind('headeronly')
-- on_load(function(target)
--     target:add('includedirs', path.join(has_config('lc_ext_path'), 'magic_enum'), {
--         public = true
--     })
-- end)
-- target_end()
