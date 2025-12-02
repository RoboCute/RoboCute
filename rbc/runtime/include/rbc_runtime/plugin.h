#pragma once
#include <luisa/vstl/meta_lib.h>

namespace rbc {
struct Plugin {
    virtual void dispose() = 0;
    virtual ~Plugin() = default;
};
}// namespace rbc

#define RBC_PLUGIN_ENTRY_NAMESPACE(NameSpace, PluginName) LUISA_EXPORT_API NameSpace::PluginName *load_rbc_plugin_##PluginName()

#define RBC_PLUGIN_ENTRY(PluginName) LUISA_EXPORT_API PluginName *load_rbc_plugin_##PluginName()

#define RBC_LOAD_PLUGIN(DllModule, PluginName) (DllModule).template invoke<PluginName*()>((luisa::string("load_rbc_plugin_") + #PluginName).c_str())