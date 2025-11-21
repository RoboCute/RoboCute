#pragma once
#include <luisa/vstl/meta_lib.h>

namespace rbc {
struct Plugin : vstd::IOperatorNewBase {
    virtual ~Plugin() = default;
};
}// namespace rbc

#define RBC_LOAD_PLUGIN_ENTRY_NAMESPACE(NameSpace, PluginName) LUISA_EXPORT_API NameSpace::PluginName *load_rbc_plugin_##PluginName()

#define RBC_LOAD_PLUGIN_ENTRY(PluginName) LUISA_EXPORT_API PluginName *load_rbc_plugin_##PluginName()
