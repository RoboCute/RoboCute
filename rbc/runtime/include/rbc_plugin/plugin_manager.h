#pragma once
#include <luisa/core/stl/vector.h>
#include <luisa/core/dynamic_module.h>
#include <luisa/vstl/functional.h>
#include <luisa/vstl/pool.h>
#include <luisa/vstl/common.h>
#include <luisa/core/fiber.h>
namespace rbc {
struct RBC_RUNTIME_API PluginManager {
private:
    vstd::HashMap<
        luisa::string,
        luisa::weak_ptr<luisa::DynamicModule>>
        loaded_modules;
    std::mutex mtx;
public:
    PluginManager();
    ~PluginManager();
    luisa::shared_ptr<luisa::DynamicModule> load_module(luisa::string_view name);
    void unload_module(luisa::string_view name);
    static PluginManager &instance();
    static void init();
    static void destroy_instance();
};
}// namespace rbc