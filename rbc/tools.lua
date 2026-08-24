target('compile_shaders')
set_kind('phony')
if not is_mode('debug') then
    add_deps('rbcxx', {inherit = false}) -- rbc clangcxx compiler
end
before_build(function(target)
    if is_mode('debug') then
        return nil
    end
    if not os.is_host('windows') then
        return nil
    end
    local builddir = path.directory(target:targetdir())
    local compiler = path.join(os.projectdir(), 'build/tool/rbcxx/rbcxx.exe')
    assert(os.isfile(compiler), 'rbcxx.exe not found. Build the `rbcxx` target first.')
    os.execv(compiler, {'--variant=build',
                        '--project-root=' .. os.projectdir(),
                        '--build-root=' .. builddir})
end)
target_end()

target('compile_shaders_hostgen')
set_kind('phony')
add_deps('compile_shaders', {
    inherit = false
})
if not is_mode('debug') then
    add_deps('rbcxx', {inherit = false}) -- rbc clangcxx compiler
end
before_build(function(target)
    if is_mode('debug') then
        return nil
    end
    if not os.is_host('windows') then
        return nil
    end
    local builddir = path.directory(target:targetdir())
    local shader_dir = path.translate(path.join(os.projectdir(), 'rbc/shader/'))
    local compiler = path.join(os.projectdir(), 'build/tool/rbcxx/rbcxx.exe')
    assert(os.isfile(compiler), 'rbcxx.exe not found. Build the `rbcxx` target first.')
    os.execv(compiler, {'--variant=build',
                        '--hostgen-only',
                        '--host-out=' .. path.translate(path.join(shader_dir, 'host')),
                        '--project-root=' .. os.projectdir(),
                        '--build-root=' .. builddir})
    os.execv(compiler, {'--variant=verify-coherence',
                        '--build-root=' .. builddir,
                        '--host-out=' .. path.translate(path.join(shader_dir, 'host'))})
end)
set_policy('build.fence', true)
target_end()
