target("test_urdf")
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    set_group("02.tests")
    add_deps('urdfdom_model', 'external_doctest')
    add_includedirs("../_framework")
    add_files("*.cpp")
    add_files("../_framework/test_util.cpp")
target_end()
