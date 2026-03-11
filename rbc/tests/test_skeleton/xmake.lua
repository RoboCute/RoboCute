target("test_skeleton")
    add_rules('lc_basic_settings', {
        project_kind = 'binary'
    })
    set_group("02.tests")
    add_interface_depend('rbc_importer_plugin')
    add_deps('rbc_runtime', 'stb-image', 'argparse')
    add_files("*.cpp")
target_end()
