#include <rbc_plugin/plugin_manager.h>
int main(int argc, char *argv[]) {
    rbc::PluginManager::init();
    auto defer_destroy = vstd::scope_exit([&] {
        rbc::PluginManager::destroy_instance();
    });
    auto rbc_editor_module = rbc::PluginManager::instance().load_module("rbc_editor_module");
    auto value = rbc_editor_module->invoke<int(int, char *[])>(
        "dll_main",
        argc,
        argv);
    return value;
}