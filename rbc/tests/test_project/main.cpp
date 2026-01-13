#include <rbc_project/project.h>
#include <rbc_project/project_plugin.h>
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
    auto binary_dir = RenderDevice::instance().lc_ctx().runtime_directory() / "test_project_meta";
    if (!luisa::filesystem::is_directory(binary_dir)) {
        luisa::filesystem::create_directories(binary_dir);
    }
    utils.init_graphics(RenderDevice::instance().lc_ctx().runtime_directory().parent_path() / (luisa::string("shader_build_") + utils.backend_name()));
    world::init_world(binary_dir);
    // TODO: test project
    auto project_plugin_module = PluginManager::instance().load_module("rbc_project_plugin");
    auto project_plugin = project_plugin_module->invoke<ProjectPlugin *()>(
        "get_project_plugin");
    auto proj = luisa::unique_ptr<IProject>(project_plugin->create_project(argv[2]));
    proj->scan_project();
    return 0;
}