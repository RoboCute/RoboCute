local function rbc_render_interface()
    add_includedirs('include', {
        public = true
    })
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
    after_build(function(target)
        local copy_opts = {
            copy_if_different = true,
            async = true,
            detach = true
        }
        os.cp(path.join(os.projectdir(), 'build/download/render_resources/*'), target:targetdir(), copy_opts)
        local dx_sdk_srcdir = path.join(os.projectdir(), 'build/download/dx_sdk')
        if os.exists(dx_sdk_srcdir) then
            os.cp(path.join(dx_sdk_srcdir, '*'), target:targetdir(), copy_opts)
        end
    end)
end

interface_target('rbc_render_plugin', rbc_render_interface, rbc_render_impl, true)
