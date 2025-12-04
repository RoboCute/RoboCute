target("oidn_checker")
add_rules("lc_basic_settings", {
    project_kind = "binary"
})
on_load(function(target)
    target:add('deps', 'lc-backends-dummy', {
        inherit = false,
        links = false
    })
    target:add('deps', 'oidn_interface', 'lc-runtime')
end)
add_files("*.cpp")
target_end()
