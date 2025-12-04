target('dylib')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'dylib')
    target:add('includedirs', path.join(src_dir, 'include'), {
        public = true
    })
    target:add('includedirs', path.join(src_dir, 'src'))
    target:add('files', path.join(src_dir, 'src/**.cpp'))
end)
target_end()
