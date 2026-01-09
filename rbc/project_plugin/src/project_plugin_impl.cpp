#include <rbc_project/project_plugin.h>

namespace rbc {

struct ProjectPluginImpl : ProjectPlugin {
public:
    ProjectPluginImpl() {}
};

LUISA_EXPORT_API ProjectPlugin *get_project_plugin() {
    static ProjectPluginImpl project_plugin_impl{};
    return &project_plugin_impl;
}

}// namespace rbc