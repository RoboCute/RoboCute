#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/services/IEventBus.h"
#include <QDebug>

namespace rbc {

PluginContext::PluginContext(EditorPluginManager *manager, QObject *parent)
    : QObject(parent)
    , manager_(manager) {
    if (!manager_) {
        qWarning() << "PluginContext: manager is null";
    }
}

IEventBus *PluginContext::event_bus() const {
    return manager_ ? manager_->getService<IEventBus>() : nullptr;
}

}// namespace rbc

