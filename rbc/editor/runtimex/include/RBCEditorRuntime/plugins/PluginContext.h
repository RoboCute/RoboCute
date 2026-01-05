#pragma once

#include <QObject>
#include "RBCEditorRuntime/plugins/PluginManager.h"
namespace rbc {

class PluginContext : public QObject {
    Q_OBJECT
public:
    explicit PluginContext(PluginManager *manager, QObject *parent);

    // Get Service
    template<typename T>
    T *getService() const {
        return qobject_cast<T *>(manager_->getService(T::staticMataObject.className()));
    }

    // fast impl for services
    IEventBus *event_bus() const;


private:
    PluginManager *manager_;
};

}// namespace rbc