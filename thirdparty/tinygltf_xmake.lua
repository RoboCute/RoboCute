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
    -- Don't define STB_IMAGE_IMPLEMENTATION in tinygltf - let stb-image library provide it
    -- But still allow tinygltf to use stb_image functions by including the header
    -- This avoids duplicate symbol definitions while allowing tinygltf to load images
end)
target_end()
