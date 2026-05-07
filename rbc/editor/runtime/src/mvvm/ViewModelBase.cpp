#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"
#include "RBCEditorRuntime/services/IEventBus.h"
#include <QDebug>

namespace rbc {

ViewModelBase::ViewModelBase(QObject *parent)
    : QObject(parent), _is_busy(false) {
}

void ViewModelBase::setBusy(bool busy) {
    if (_is_busy != busy) {
        _is_busy = busy;
        emit isBusyChanged();
    }
}

void ViewModelBase::setError(const QString &message) {
    if (_error_message != message) {
        _error_message = message;
        emit errorMessageChanged();
    }
}

void ViewModelBase::clearError() {
    if (!_error_message.isEmpty()) {
        _error_message.clear();
        emit errorMessageChanged();
    }
}

void ViewModelBase::publish(const Event &event) {
    // TODO: Get EventBus from PluginManager
    // For now, this is a placeholder
    Q_UNUSED(event);
}

void ViewModelBase::subscribe(EventType type, std::function<void(const Event &)> handler) {
    // TODO: Implement event subscription
    Q_UNUSED(type);
    Q_UNUSED(handler);
}

void ViewModelBase::unsubscribe(EventType type) {
    // TODO: Implement event unsubscription
    Q_UNUSED(type);
}

}// namespace rbc
