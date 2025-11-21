#include <luisa/core/stl.h>
#include <luisa/core/basic_traits.h>
#include <luisa/core/basic_types.h>
#include <luisa/core/logging.h>
#include <luisa/core/fiber.h>
#include <luisa/core/magic_enum.h>
#include <rbc_runtime/plugin_manager.h>
#include <rbc_runtime/plugin_module.h>
#include <luisa/vstl/md5.h>
using namespace rbc;
using namespace luisa;
int main() {
    LUISA_INFO("Start test graph");
    luisa::fiber::scheduler scheduler;
    PluginManager plugin_manager;
    vstd::HashMap<PluginModule *, vstd::string> modules;
    auto a = modules.try_emplace(plugin_manager.load_module(), "A").first.key();
    auto b = modules.try_emplace(plugin_manager.load_module(), "B").first.key();
    auto c = modules.try_emplace(plugin_manager.load_module(), "C").first.key();
    auto d = modules.try_emplace(plugin_manager.load_module(), "D").first.key();
    PluginModule::add_depend(a, b);
    PluginModule::add_depend(a, c);
    PluginModule::add_depend(b, d);
    PluginModule::add_depend(c, d);
    auto graph = plugin_manager.compile();
    graph.execute_parallel([&](PluginModule *module) {
        LUISA_INFO("{}", modules.find(module).value());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    });
}