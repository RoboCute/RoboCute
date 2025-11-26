target("sync_editor")
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    add_files("main.cpp", "http_client.cpp", "scene_sync.cpp", "editor_scene.cpp")
    add_headerfiles("http_client.h", "scene_sync.h", "editor_scene.h")
    
    -- Dependencies
    add_deps("rbc_core")  -- For json_serde (yyjson)
    add_deps("rbc_world")
    add_deps('rbc_runtime', 'lc-gui', 'rbc_render_interface')
    
    -- System libraries for WinHTTP
    if is_plat("windows") then
        add_syslinks("winhttp")
    end
target_end()