rule('_rbc_depend_')
on_load(function(target)
    local no_link = target:extraconf('rules', '_rbc_depend_', 'no_link')
    local target_name = target:extraconf('rules', '_rbc_depend_', 'target_name')
    local target_impl_name = target:extraconf('rules', '_rbc_depend_', 'target_impl_name')
    local target_interface_name = target:extraconf('rules', '_rbc_depend_', 'target_interface_name')
    target:add('deps', target_impl_name, {
        inherit = false,
        links = not no_link
    })
    target:add('deps', target_interface_name, {
        links = false
    })
    if not no_link then
        target:add('links', target_name, {
            public = true
        })
    end
end)
rule_end()
function interface_target(target_name, interface_callback, impl_callback, no_link)
    local target_interface_name = target_name .. '_int__'
    local target_impl_name = target_name .. '_impl__'
    target(target_interface_name)
    set_kind('phony')
    interface_callback()
    target_end()

    target(target_impl_name)
    set_basename(target_name)
    add_deps(target_interface_name)
    impl_callback()
    target_end()

    target(target_name)
    set_kind('phony')
    add_rules('lc_run_target')
    add_rules('_rbc_depend_', {
        no_link = no_link or false,
        target_name = target_name,
        target_impl_name = target_impl_name,
        target_interface_name = target_interface_name
    })
end
