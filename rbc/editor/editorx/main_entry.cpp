#include <rbc_plugin/plugin_manager.h>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
    rbc::PluginManager::init();
    auto defer_destroy = vstd::scope_exit([&] {
        rbc::PluginManager::destroy_instance();
    });
    
    // Parse command line arguments and remove -t if present
    bool use_testbed = false;
    std::vector<char*> filtered_argv;
    filtered_argv.push_back(argv[0]); // Always include program name
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-t") {
            use_testbed = true;
            // Skip -t, don't add it to filtered_argv
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    
    int filtered_argc = static_cast<int>(filtered_argv.size());
    
    // Load appropriate module based on arguments
    std::string module_name = use_testbed ? "rbc_testbed_module" : "rbc_editor_module";
    auto module = rbc::PluginManager::instance().load_module(module_name);
    auto value = module->invoke<int(int, char *[])>(
        "dll_main",
        filtered_argc,
        filtered_argv.data());
    return value;
}