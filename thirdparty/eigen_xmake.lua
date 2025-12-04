target('eigen')
set_kind('headeronly')
on_load(function(target)
    local src_dir = path.join(os.scriptdir(), 'eigen')
    target:add('includedirs', src_dir, {
        public = true
    })
    target:add('defines', 'EIGEN_HAS_STD_RESULT_OF=0', {
        public = true
    })
end)
on_config(function(target)
    local _, cc = target:tool("cxx")
    if (cc == "clang" or cc == "clangxx") then
        target:add("defines", "EIGEN_DISABLE_AVX", {
            public = true
        })
    end
end)
target_end()
