#include "RBCEditorRuntime/plugins/ViewportPlugin.h"

namespace rbc {
ViewportPlugin::ViewportPlugin(QObject *parent) {}
ViewportPlugin::~ViewportPlugin() {}

bool ViewportPlugin::load(PluginContext *context) {
    return false;
}
bool ViewportPlugin::unload() {
    return false;
}
bool ViewportPlugin::reload() {
    return false;
}

QObject *ViewportPlugin::getViewModel(const QString &viewId) {
    if (viewId == "viewport" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

}// namespace rbc