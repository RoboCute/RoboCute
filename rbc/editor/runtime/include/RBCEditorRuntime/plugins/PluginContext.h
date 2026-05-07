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
    [[nodiscard]] T *getService() const {
        return _manager->getService<T>();
    }

    // fast impl for services
    [[nodiscard]] IEventBus *event_bus() const;


private:
    EditorPluginManager *_manager;
};

}// namespace rbc