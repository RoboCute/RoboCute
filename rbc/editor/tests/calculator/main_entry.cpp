#include <rbc_plugin/plugin_manager.h>
int main(int argc, char *argv[]) {
    rbc::PluginManager::init();
    auto defer_destroy = vstd::scope_exit([&] {
        rbc::PluginManager::destroy_instance();
    });
    auto calculator_module = rbc::PluginManager::instance().load_module("calculator_module");
    auto value = calculator_module->invoke<int(int, char *[])>(
        "dll_main",
        argc,
        argv);
    return value;
}