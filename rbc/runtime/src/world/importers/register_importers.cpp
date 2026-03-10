#include <rbc_world/resource_importer.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/runtime_static.h>
namespace rbc::world {

/**
 * @brief Register all built-in resource importers
 * This function should be called during system initialization
 */
struct ImporterPluginStatic {
    luisa::shared_ptr<luisa::DynamicModule> importer_plugin;
};
static RuntimeStatic<ImporterPluginStatic> _importer_plug;
RBC_RUNTIME_API void register_builtin_importers() {
    // auto &registry = ResourceImporterRegistry::instance();
    _importer_plug->importer_plugin = PluginManager::instance().load_module("rbc_importer_plugin");
    if (_importer_plug->importer_plugin && *_importer_plug->importer_plugin)
        _importer_plug->importer_plugin->invoke<void()>(
            "register_builtin_importers");
    // Anim Resources
}

}// namespace rbc::world
