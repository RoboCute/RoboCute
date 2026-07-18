target('compile_shaders')
set_kind('phony')
before_build(function(target)
    if not os.is_host('windows') then
        return nil
    end
    local builddir = path.directory(target:targetdir())
    local find_tool = import('lib.detect.find_tool')
    local uv = find_tool('uv')
    assert(uv, 'uv is required to build shaders. Run `uv sync --extra=all` first.')
    os.execv(uv.program, {'run', 'shader-build', 'build',
                          '--project-root', os.projectdir(),
                          '--build-root', builddir})
end)
target_end()

target('compile_shaders_hostgen')
set_kind('phony')
add_deps('compile_shaders', {
    inherit = false
})
before_build(function(target)
    if not os.is_host('windows') then
        return nil
    end
    local builddir = path.directory(target:targetdir())
    local shader_dir = path.translate(path.join(os.projectdir(), 'rbc/shader/'))
    local find_tool = import('lib.detect.find_tool')
    local uv = find_tool('uv')
    assert(uv, 'uv is required to generate shader host headers. Run `uv sync --extra=all` first.')
    os.execv(uv.program, {'run', 'shader-build', 'build',
                          '--project-root', os.projectdir(),
                          '--build-root', builddir,
                          '--hostgen-only',
                          '--host-out', path.translate(path.join(shader_dir, 'host'))})
    os.execv(uv.program, {'run', 'shader-build', 'verify-coherence',
                          '--project-root', os.projectdir(),
                          '--build-root', builddir,
                          '--host-out', path.translate(path.join(shader_dir, 'host'))})
end)
set_policy('build.fence', true)
target_end()
