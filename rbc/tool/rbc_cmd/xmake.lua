-- The 'rbc' commandline tool
target("rbc")
add_rules('lc_basic_settings', {
    project_kind = 'binary',
    enable_exception = true,
    rtti = true
})
set_group("04.targets")
add_files("main.cpp")
add_deps("rbc_runtime")
add_deps("argparse")
target_end()
