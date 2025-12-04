target('cpptrace')
add_rules('lc_basic_settings', {
    project_kind = 'shared'
})
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'cpptrace')
    target:add('includedirs', path.join(src_dir, 'include'), {
        public = true
    })
    target:add('includedirs', path.join(src_dir, 'src'))
    target:add('files', path.join(src_dir, 'src/**.cpp'))
    target:add('defines', 'cpptrace_lib_EXPORTS', 'CPPTRACE_UNWIND_WITH_NOTHING', 'CPPTRACE_DEMANGLE_WITH_NOTHING')
end)
target_end()
