rule('_rbc_depend_')
on_load(function(target)
    local no_link = target:extraconf('rules', '_rbc_depend_', 'no_link')
    local target_name = target:extraconf('rules', '_rbc_depend_', 'target_name')
    local target_interface_name = target:extraconf('rules', '_rbc_depend_', 'target_interface_name')
    local target_impl_name = target:extraconf('rules', '_rbc_depend_', 'target_impl_name')
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
if has_config('rbc_plugins') then
    function interface_target(target_name, interface_callback, impl_callback, is_plugin)
        local target_interface_name = target_name .. '_int__'
        target(target_interface_name)
        set_kind('phony')
        interface_callback()
        target_end()

        local target_impl_name = target_name .. '_impl__'
        target(target_impl_name)
        set_basename(target_name)
        add_deps(target_interface_name)
        impl_callback()
        target_end()

        target(target_name)
        set_kind('phony')
        add_rules('lc_run_target')
        add_rules('_rbc_depend_', {
            no_link = is_plugin or false,
            target_name = target_name,
            target_impl_name = target_impl_name,
            target_interface_name = target_interface_name
        })
    end
else
    function interface_target(target_name, interface_callback, impl_callback, is_plugin)
        local target_interface_name = target_name .. '_int__'
        target(target_interface_name)
        set_kind('phony')
        interface_callback()
        target_end()

        if (not is_plugin) then

            local target_impl_name = target_name .. '_impl__'
            target(target_impl_name)
            set_basename(target_name)
            add_deps(target_interface_name)
            impl_callback()
            target_end()

            target(target_name)
            set_kind('phony')
            add_rules('lc_run_target')
            add_rules('_rbc_depend_', {
                no_link = is_plugin or false,
                target_name = target_name,
                target_impl_name = target_impl_name,
                target_interface_name = target_interface_name
            })
        else
            target(target_name)
            set_kind('phony')
            add_deps(target_interface_name)
            target_end()
        end
    end
end

function add_interface_depend(target, options)
    add_deps(target .. '_int__', options)
end
