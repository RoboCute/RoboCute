target("install_clangcxx")
set_kind("phony")
add_rules('lc_install_sdk', {
    sdk_dir = 'build/download',
    libnames = {
        name = 'clangcxx_compiler-v2.0.1.7z',
        plat_spec = true,
        -- download in python
        -- address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/',
        copy_dir = '',
        extract_dir = 'build/tool/clangcxx_compiler'

    }
})
set_policy('build.fence', true)
target_end()

target("install_tools")
set_kind("phony")
add_rules('lc_install_sdk', {
    sdk_dir = 'build/download',
    libnames = {{
        name = 'clangd-v19.1.7.7z',
        plat_spec = true,
        -- download in python
        -- address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/',
        copy_dir = '',
        extract_dir = 'build/tool/clangd'
    } -- , {
    --     name = 'render_resources.7z',
    --     address = 'https://github.com/RoboCute/RoboCute.Resouces/releases/download/Release/'
    -- }
    }
})
set_policy('build.fence', true)
target_end()

target("compile_shaders")
set_kind("phony")
add_deps("install_clangcxx", {
    inherit = false,
    public = false
})
before_build(function(target)
    local builddir = path.directory(target:targetdir())
    local compiler_path = "clangcxx_compiler"
    if os.is_host("windows") then
        compiler_path = compiler_path .. ".exe"
    end
    compiler_path = path.join(os.projectdir(), "build/tool/clangcxx_compiler", compiler_path)
    local shader_dir = path.translate(path.join(os.projectdir(), "rbc/shader/"))
    local backends = {'dx', 'vk'}
    import("async.jobgraph")
    import("async.runjobs")
    local jobs = jobgraph.new()
    for i, v in ipairs(backends) do
        jobs:add(v, function()
            local cache_dir = path.join(shader_dir, '.cache', v)
            local shader_out_dir = path.translate(path.join(builddir, "shader_build_" .. v))
            local args = {"--in=" .. path.translate(path.join(shader_dir, "src")), "--out=" .. shader_out_dir,
                          "--cache_dir=" .. cache_dir, "--backend=" .. v,
                          "--include=" .. path.translate(path.join(shader_dir, "include"))}
            if i == 1 then
                table.insert(args, "--hostgen=" .. path.translate(path.join(shader_dir, "host")))
            end
            os.mkdir(cache_dir)
            os.execv(compiler_path, args)
        end)
    end
    runjobs("compile", jobs, {
        comax = #backends
    })
end)
set_policy('build.fence', true)
target_end()
