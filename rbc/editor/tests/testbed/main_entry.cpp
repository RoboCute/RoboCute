#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/runtime_static.h>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
    luisa::fiber::scheduler scheduler;
    rbc::RuntimeStaticBase::init_all();
    rbc::PluginManager::init();

    auto defer_destroy = vstd::scope_exit([&] {
        rbc::PluginManager::destroy_instance();
        rbc::RuntimeStaticBase::dispose_all();
    });

    // Parse command line arguments and remove mode flags if present
    bool render_testbed = false;
    bool graph_testbed = false;
    std::vector<char *> filtered_argv;
    filtered_argv.push_back(argv[0]);// Always include program name

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-r" || arg == "--render") {
            render_testbed = true;
        } else if (arg == "-g" || arg == "--graph") {
            graph_testbed = true;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }

    int filtered_argc = static_cast<int>(filtered_argv.size());

    // Load appropriate module based on arguments
    std::string module_name = "rbc_storybook_module";
    if (render_testbed) {
        module_name = "rbc_render_testbed_module";
    } else if (graph_testbed) {
        module_name = "rbc_graph_testbed_module";
    }
    auto module = rbc::PluginManager::instance().load_module(module_name);
    auto value = module->invoke<int(int, char *[])>(
        "dll_main",
        filtered_argc,
        filtered_argv.data());
    return value;
}