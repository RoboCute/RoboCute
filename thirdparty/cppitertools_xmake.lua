target('cppitertools')
set_kind('headeronly')
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'cppitertools')
    target:add('includedirs', src_dir, {
        public = true
    })
end)
target_end()
