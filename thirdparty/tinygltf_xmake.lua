target('tinygltf')
add_rules('lc_basic_settings', {
    project_kind = 'static'
})
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'tinygltf')
    target:add('includedirs', src_dir, {
        public = true
    })
    target:add('files', path.join(src_dir, '**.cc'))
end)
target_end()
