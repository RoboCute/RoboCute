#pragma once

#include <QObject>
#include "RBCEditorRuntime/services/IEventBus.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API PluginContext : public QObject {
    Q_OBJECT
public:
    explicit PluginContext(EditorPluginManager *manager, QObject *parent);

    // Get Service
    template<typename T>
    T *getService() const {
        return qobject_cast<T *>(manager_->getService(T::staticMetaObject.className()));
    }

    // fast impl for services
    IEventBus *event_bus() const;


private:
    EditorPluginManager *manager_;
};

}// namespace rbc