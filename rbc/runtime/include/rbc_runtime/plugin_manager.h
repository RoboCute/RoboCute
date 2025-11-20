#pragma once
#include <luisa/core/stl/vector.h>
#include <rbc_runtime/plugin_module.h>
namespace rbc {
struct PluginManager {
    luisa::vector<RC<PluginModule>> loaded_modules;
    
};
}// namespace rbc