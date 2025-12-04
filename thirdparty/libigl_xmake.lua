target('libigl')
set_kind('headeronly')
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'libigl')
    target:add('includedirs', path.join(src_dir, 'include'), {
        public = true
    })
end)
target_end()
