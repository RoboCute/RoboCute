-- urdfdom_headers: header-only library with URDF data structures
target('urdfdom_headers')
do
    on_load(function(target)
        target:set('kind', 'headeronly')
        target:add('includedirs', path.join(os.scriptdir(), 'urdfdom_headers/include'), {
            public = true
        })
    end)
end

-- tinyxml2: XML parsing library
target('tinyxml2')
do
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    on_load(function(target)
        target:add('files', path.join(os.scriptdir(), 'tinyxml2/tinyxml2.cpp'))
        target:add('includedirs', path.join(os.scriptdir(), 'tinyxml2'), {
            public = true
        })
        target:add('defines', 'TINYXML2_EXPORT', {
            public = true
        })
    end)
end

-- console_bridge: logging library for ROS
target('console_bridge')
do
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    on_load(function(target)
        target:add('files', path.join(os.scriptdir(), 'console_bridge/src/console.cpp'))
        target:add('includedirs', path.join(os.scriptdir(), 'console_bridge/include'), {
            public = true
        })
        -- Export header defines for static linking (avoid need for generated export header)
        target:add('defines', 'CONSOLE_BRIDGE_STATIC', 'CONSOLE_BRIDGE_DLLAPI', {
            public = true
        })
    end)
end

-- urdfdom_model: core URDF model parsing
target('urdfdom_model')
do
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    on_load(function(target)
        target:add('deps', 'urdfdom_headers', 'tinyxml2', 'console_bridge')
        target:add('includedirs', path.join(os.scriptdir(), 'urdfdom/urdf_parser/include'), {
            public = true
        })
        target:add('files', path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/pose.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/model.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/link.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/joint.cpp'))
        -- Export defines for static linking
        target:add('defines', 'URDFDOM_STATIC', {
            public = true
        })
    end)
end

-- urdfdom_world: URDF world parsing (includes model + world)
target('urdfdom_world')
do
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    on_load(function(target)
        target:add('deps', 'urdfdom_headers', 'tinyxml2', 'console_bridge')
        target:add('includedirs', path.join(os.scriptdir(), 'urdfdom/urdf_parser/include'), {
            public = true
        })
        target:add('files', path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/pose.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/model.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/link.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/joint.cpp'),
            path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/world.cpp'))
        -- Export defines for static linking
        target:add('defines', 'URDFDOM_STATIC', {
            public = true
        })
    end)
end

-- urdfdom_sensor: URDF sensor parsing
target('urdfdom_sensor')
do
    add_rules('lc_basic_settings', {
        project_kind = 'static'
    })
    on_load(function(target)
        target:add('deps', 'urdfdom_model')
        target:add('includedirs', path.join(os.scriptdir(), 'urdfdom/urdf_parser/include'), {
            public = true
        })
        target:add('files', path.join(os.scriptdir(), 'urdfdom/urdf_parser/src/urdf_sensor.cpp'))
        -- Export defines for static linking
        target:add('defines', 'URDFDOM_STATIC', {
            public = true
        })
    end)
end

-- urdfdom: combined interface target (combines all components)
target('urdfdom')
do
    on_load(function(target)
        target:set('kind', 'phony')
        target:add('deps', 'urdfdom_model', 'urdfdom_world', 'urdfdom_sensor')
        target:add('includedirs', path.join(os.scriptdir(), 'urdfdom/urdf_parser/include'), {
            public = true
        })
    end)
end
