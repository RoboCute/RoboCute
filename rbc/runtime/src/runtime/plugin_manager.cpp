#pragma once
#include <luisa/core/shared_function.h>
#include <rbc_runtime/plugin_manager.h>
namespace rbc {
namespace plugin_mng_detail {
vstd::optional<PluginManager> inst_;
}// namespace plugin_mng_detail
PluginManager &PluginManager::instance() {
    return *plugin_mng_detail::inst_;
}
void PluginManager::init() {
    if (!plugin_mng_detail::inst_)
        plugin_mng_detail::inst_.create();
}
void PluginManager::destroy_instance() {
    if (plugin_mng_detail::inst_)
        plugin_mng_detail::inst_.destroy();
}
PluginManager::PluginManager() {}
PluginManager::~PluginManager() {}
void PluginManager::unload_module(luisa::string_view name) {
    std::lock_guard lck{mtx};
    loaded_modules.remove(name);
}
luisa::DynamicModule const &PluginManager::load_module(luisa::string_view name) {
    std::lock_guard lck{mtx};
    return loaded_modules.try_emplace(
                             name,
                             vstd::lazy_eval([&]() {
                                 return luisa::DynamicModule::load(name);
                             }))
        .first.value();
}
}// namespace rbc