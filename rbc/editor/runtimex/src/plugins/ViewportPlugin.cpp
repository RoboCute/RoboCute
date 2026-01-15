#include "RBCEditorRuntime/plugins/ViewportPlugin.h"

namespace rbc {

ViewportPlugin::ViewportPlugin(QObject *parent) {}
ViewportPlugin::~ViewportPlugin() {}

bool ViewportPlugin::load(PluginContext *context) {
    if (!context) {
        qWarning() << "ViewportPlugin::load context is null";
        return false;
    }
    context_ = context;
    // TODO: Get SceneService from PluginManager

    // Create ViewModel

    return true;
}

bool ViewportPlugin::unload() {
    return true;
}
bool ViewportPlugin::reload() {
    return true;
}

QObject *ViewportPlugin::getViewModel(const QString &viewId) {
    if (viewId == "viewport" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

}// namespace rbc