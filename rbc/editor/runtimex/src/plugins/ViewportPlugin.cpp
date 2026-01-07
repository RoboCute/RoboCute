#include "RBCEditorRuntime/plugins/ViewportPlugin.h"

namespace rbc {
ViewportPlugin::ViewportPlugin(QObject *parent) {}
ViewportPlugin::~ViewportPlugin() {}

bool ViewportPlugin::load(PluginContext *context) {}
bool ViewportPlugin::unload() {}
bool ViewportPlugin::reload() {}

QObject *ViewportPlugin::getViewModel(const QString &viewId) {
    if (viewId == "viewport" && viewModel_) {
        return viewModel_;
    }
    return nullptr;
}

}// namespace rbc