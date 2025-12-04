target('nlohmann_json')
set_kind('headeronly')
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'nlohmann_json')
    target:add('includedirs', path.join(src_dir, 'include'), {
        public = true
    })
end)
target_end()
