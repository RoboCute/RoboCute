local function rbc_ext_c_interface()
    add_includedirs("include", {
        public = true
    })
end

local function rbc_ext_c_impl()
    add_rules('lc_basic_settings', {
        project_kind = 'shared',
        enable_exception = true,
        rtti = true
    })
    add_deps('lc-core')
    add_rules('pybind')
    set_extension('.pyd')
    add_deps("rbc_world_v2")
    add_files("src/*.cpp")
end

interface_target('rbc_ext_c', rbc_ext_c_interface, rbc_ext_c_impl)
