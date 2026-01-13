option('rbc_py_bin', {
    default = false,
    showmenu = false
})

-- enable targets

option('rbc_editor', {
    default = true,
    showmenu = false
})

option('rbc_tests', {
    default = true,
    showmenu = false
})

option('rbc_tools', {
    default = true,
    showmenu = false
})

option('rbc_plugins', {
    default = true,
    showmenu = false
})

option('rbc_unity_build', {
    default = true,
    showmenu = false
})

option('rbc_pch', {
    default = true,
    showmenu = false
})

option('rbc_option')
set_showmenu(false)
set_default(false)
add_deps('lc_toolchain', 'lc_py_include', 'lc_py_linkdir', 'lc_py_libs', 'rbc_py_bin', 'lc_use_lto', 'rbc_editor',
    'rbc_unity_build', 'rbc_pch', 'rbc_tools', 'rbc_plugins', 'rbc_tests')
after_check(function(option)
    import('core.base.json')
    local luatable = json.decode(io.readfile(path.join(os.scriptdir(), 'options.json')))
    for k, v in pairs(luatable) do
        option:dep(k):enable(v, {
            force = true
        })
    end
    if has_config('toolchain') == 'msvc' and is_mode('release') then
        option:dep('lc_use_lto'):enable(true, {
            force = true
        })
    end
end)
option_end()

if has_config('rbc_pch') then
    function rbc_set_pch(file)
        set_pcxxheader(file)
    end
else
    function rbc_set_pch(file)
    end
end

if has_config('rbc_unity_build') then
    function rbc_unity_build(batchsize)
        if batchsize > 0 then
            add_rules('c++.unity_build', {
                batchsize = batchsize
            })
        end
    end
else
    function rbc_unity_build(batchsize)
    end
end
