target('compile_shaders')
set_kind('phony')
before_build(function(target)
    if not os.is_host('windows') then
        return nil
    end
    local builddir = path.directory(target:targetdir())
    local compiler_path = 'clangcxx_compiler'
    if os.is_host('windows') then
        compiler_path = compiler_path .. '.exe'
    end
    compiler_path = path.join(os.projectdir(), 'build/tool/clangcxx_compiler', compiler_path)
    local shader_dir = path.translate(path.join(os.projectdir(), 'rbc/shader/'))
    local backends = {'dx', 'vk'}
    import('async.jobgraph')
    import('async.runjobs')
    local jobs = jobgraph.new()
    for i, v in ipairs(backends) do
        jobs:add(v, function()
            local cache_dir = path.join(shader_dir, '.cache', v)
            local shader_out_dir = path.translate(path.join(builddir, 'shader_build_' .. v))
            local args = {'--in=' .. path.translate(path.join(shader_dir, 'src')), '--out=' .. shader_out_dir,
                          '--cache_dir=' .. cache_dir, '--backend=' .. v,
                          '--include=' .. path.translate(path.join(shader_dir, 'include'))}
            -- if i == 1 then
            --     table.insert(args, '--hostgen=' .. path.translate(path.join(shader_dir, 'host')))
            -- end
            os.mkdir(cache_dir)
            os.execv(compiler_path, args)
        end)
    end
    runjobs('compile', jobs, {
        comax = #backends
    })
end)
target_end()

target('compile_shaders_hostgen')
set_kind('phony')
before_build(function(target)
    if not os.is_host('windows') then
        return nil
    end
    local builddir = path.directory(target:targetdir())
    local compiler_path = 'clangcxx_compiler'
    if os.is_host('windows') then
        compiler_path = compiler_path .. '.exe'
    end
    compiler_path = path.join(os.projectdir(), 'build/tool/clangcxx_compiler', compiler_path)
    local shader_dir = path.translate(path.join(os.projectdir(), 'rbc/shader/'))
    local cache_dir = path.join(shader_dir, '.cache', 'hostgen')
    local shader_out_dir = path.translate(path.join(builddir, 'shader_build_hostgen'))
    local args = {'--in=' .. path.translate(path.join(shader_dir, 'src')), '--out=' .. shader_out_dir,
                  '--cache_dir=' .. cache_dir, '--hostgen=' .. path.translate(path.join(shader_dir, 'host')),
                  '--include=' .. path.translate(path.join(shader_dir, 'include'))}
    os.mkdir(cache_dir)
    os.execv(compiler_path, args)
end)
set_policy('build.fence', true)
target_end()
