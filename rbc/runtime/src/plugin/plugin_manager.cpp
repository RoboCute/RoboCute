#pragma once
#include <luisa/core/shared_function.h>
#include <rbc_plugin/plugin_manager.h>
namespace rbc {
namespace plugin_mng_detail {
PluginManager *inst_{};
}// namespace plugin_mng_detail
PluginManager &PluginManager::instance() {
    return *plugin_mng_detail::inst_;
}
void PluginManager::init() {
    if (!plugin_mng_detail::inst_)
        plugin_mng_detail::inst_ = new PluginManager{};
}
void PluginManager::destroy_instance() {
    delete plugin_mng_detail::inst_;
    plugin_mng_detail::inst_ = nullptr;
}
PluginManager::PluginManager() {}
PluginManager::~PluginManager() {}
void PluginManager::unload_module(luisa::string_view name) {
    std::lock_guard lck{mtx};
    loaded_modules.remove(name);
}
luisa::shared_ptr<luisa::DynamicModule> PluginManager::load_module(luisa::string_view name) {
    std::lock_guard lck{mtx};
    auto iter = loaded_modules.try_emplace(name);
    luisa::shared_ptr<luisa::DynamicModule> r;
    if (!iter.second) {
        r = iter.first.value().lock();
    }
    if (!r) {
        r = luisa::make_shared<luisa::DynamicModule>(luisa::DynamicModule::load(name));
        iter.first.value() = r;
    }
    return r;
}
}// namespace rbc