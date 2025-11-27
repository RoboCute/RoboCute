function interface_target(target_name, interface_callback, impl_callback, no_link)
    local target_interface_name = target_name .. '_int__'
    local target_impl_name = target_name .. '_impl__'
    target(target_interface_name)
    set_kind('headeronly')
    interface_callback()
    target_end()

    target(target_impl_name)
    set_basename(target_name)
    add_deps(target_interface_name)
    impl_callback()
    target_end()

    target(target_name)
    set_kind('phony')
    add_deps(target_interface_name)
    add_deps(target_impl_name, {
        inherit = false
    })
    if not no_link then
        add_links(target_name, {
            public = true
        })
    end
    target_end()

end
