local function rbc_render_interface()
    add_includedirs('include', {
        public = true
    })
end

local shader_input_marker_name = 'rbc_render_plugin.input_id'

local function parse_shader_input_id(contents)
    return contents:match('^%s*([0-9a-f]+)%s*$')
end

local function rbc_render_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared'
    })
    rbc_set_pch('src/zz_pch.h')
    add_deps('rbc_runtime')
    add_deps('compile_shaders_hostgen', {
        inherit = false
    })
    if has_config('rbc_oidn') then
        add_deps('oidn_plugin', {
            links = false
        })
        add_defines('RBC_RENDER_ENABLE_OIDN')
    end
    add_includedirs('../shader/host', {
        public = true
    })
    add_deps('lc-backends-dummy', {
        inherit = false,
        links = false
    })
    add_files('src/**.cpp')

    -- bin 2 obj
    add_rules('utils.bin2obj', {
        extensions = {'.json', '.bytes'}
    })
    add_files('src/render_settings.json')
    before_build(function(target)
        if not os.is_host('windows') then
            return nil
        end
        local host_marker = path.join(os.projectdir(), 'rbc/shader/host/.shader_input_id')
        local marker_contents = assert(io.readfile(host_marker),
                                       'shader input marker is missing: ' .. host_marker)
        local input_id = parse_shader_input_id(marker_contents)
        assert(input_id and #input_id == 64,
               'shader input marker is invalid: ' .. host_marker)
        local plugin_marker = path.join(target:targetdir(), shader_input_marker_name)
        local linked_input_id = nil
        if os.isfile(plugin_marker) then
            linked_input_id = parse_shader_input_id(io.readfile(plugin_marker))
        end
        if not os.isfile(target:targetfile()) or linked_input_id ~= input_id then
            target:data_set('rebuilt', true)
        end
        target:data_set('rbc.shader_input_id', input_id)
    end)
    after_link(function(target)
        if os.is_host('windows') then
            local host_marker = path.join(os.projectdir(), 'rbc/shader/host/.shader_input_id')
            local captured_input_id = assert(target:data('rbc.shader_input_id'),
                                             'shader input generation was not captured before build')
            local marker_contents = assert(io.readfile(host_marker),
                                           'shader input marker is missing: ' .. host_marker)
            local current_input_id = parse_shader_input_id(marker_contents)
            assert(current_input_id and #current_input_id == 64,
                   'shader input marker is invalid: ' .. host_marker)
            assert(current_input_id == captured_input_id,
                   'shader inputs changed while rbc_render_plugin was being built; rebuild required')
            local plugin_marker = path.join(target:targetdir(), shader_input_marker_name)
            local linked_input_id = nil
            if os.isfile(plugin_marker) then
                linked_input_id = parse_shader_input_id(io.readfile(plugin_marker))
            end
            if linked_input_id ~= captured_input_id then
                io.writefile(plugin_marker, captured_input_id .. '\n')
            end
        end
    end)
    after_build(function(target)
        local copy_opts = {
            copy_if_different = true
        }
        os.cp(path.join(os.projectdir(), 'build/download/render_resources/*'), target:targetdir(), copy_opts)
        local dx_sdk_srcdir = path.join(os.projectdir(), 'build/download/dx_sdk')
        if os.exists(dx_sdk_srcdir) then
            os.cp(path.join(dx_sdk_srcdir, '*'), target:targetdir(), copy_opts)
        end
    end)
end

interface_target('rbc_render_plugin', rbc_render_interface, rbc_render_impl, true)
