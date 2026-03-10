target("test_model")
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    set_group("02.tests")
    add_interface_depend('rbc_importer_plugin')
    add_deps('rbc_runtime', 'external_doctest')
    add_includedirs("../_framework")
    add_files("*.cpp")
    add_files("../_framework/test_util.cpp")
target_end()
