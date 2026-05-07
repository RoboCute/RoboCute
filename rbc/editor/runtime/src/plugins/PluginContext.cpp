#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/services/IEventBus.h"
#include <QDebug>

namespace rbc {

PluginContext::PluginContext(EditorPluginManager *manager, QObject *parent)
    : QObject(parent)
    , _manager(manager) {
    if (!_manager) {
        qWarning() << "PluginContext: manager is null";
    }
}

IEventBus *PluginContext::event_bus() const {
    return _manager ? _manager->getService<IEventBus>() : nullptr;
}

}// namespace rbc

