option('rbc_py_bin')
set_showmenu(false)
set_default(false)
option_end()

option('rbc_py_toolchain')
set_showmenu(false)
set_default(false)
option_end()

option('rbc_option')
set_showmenu(false)
set_default(false)
add_deps('lc_toolchain', 'lc_py_include', 'lc_py_linkdir', 'lc_py_libs', 'rbc_py_bin', 'rbc_py_toolchain')
after_check(function(option)
    import("core.base.json")
    local luatable = json.decode(io.readfile(path.join(os.scriptdir(), 'options.json')))
    for k, v in pairs(luatable) do
        option:dep(k):enable(v, {
            force = true
        })
    end
end)
option_end()
