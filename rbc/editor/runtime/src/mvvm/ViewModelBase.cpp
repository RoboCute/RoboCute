#include "RBCEditorRuntime/mvvm/ViewModelBase.h"
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"
#include "RBCEditorRuntime/services/IEventBus.h"
#include <QDebug>

namespace rbc {

ViewModelBase::ViewModelBase(QObject *parent)
    : QObject(parent), isBusy_(false) {
}

void ViewModelBase::setBusy(bool busy) {
    if (isBusy_ != busy) {
        isBusy_ = busy;
        emit isBusyChanged();
    }
}

void ViewModelBase::setError(const QString &message) {
    if (errorMessage_ != message) {
        errorMessage_ = message;
        emit errorMessageChanged();
    }
}

void ViewModelBase::clearError() {
    if (!errorMessage_.isEmpty()) {
        errorMessage_.clear();
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
