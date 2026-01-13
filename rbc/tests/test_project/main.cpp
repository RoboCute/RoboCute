#include <rbc_project/project.h>
#include <luisa/core/fiber.h>
#include <rbc_plugin/plugin.h>
#include <rbc_plugin/plugin_manager.h>
#include <rbc_core/runtime_static.h>
#include <rbc_world/base_object.h>
#include <rbc_graphics/graphics_utils.h>
int main(int argc, char *argv[]) {
    if (argc < 3) {
        LUISA_WARNING("Bad args, must be #backend# #scene path#");
        return 1;
    }
    using namespace luisa;
    using namespace rbc;
    luisa::fiber::scheduler scheduler;
    GraphicsUtils utils;
    auto dispose_runtime_static = vstd::scope_exit([&] {
        utils.dispose([&]() {
            world::destroy_world();
        });
        PluginManager::destroy_instance();
        RuntimeStaticBase::dispose_all();
    });
    RuntimeStaticBase::init_all();
    PluginManager::init();
    luisa::string backend = argv[1];
    utils.init_device(
        argv[0],
        backend.c_str());
    utils.init_graphics(RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name()));
    world::init_world(argv[2]);
    // TODO: test project
    return 0;
}