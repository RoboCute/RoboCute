#include "RBCEditorRuntime/mvvm/ViewModelBase.h"

namespace rbc {

ViewModelBase::ViewModelBase(QObject *parent) {
}

void ViewModelBase::setBusy(bool busy) {
    isBusy_ = busy;
}

void ViewModelBase::setError(const QString &message) {
}

void ViewModelBase::clearError() {
}

void ViewModelBase::publish(const Event &event) {}

void ViewModelBase::subscribe(EventType type, std::function<void(const Event &)> handler) {
}

void ViewModelBase::unsubscribe(EventType type) {
}

}// namespace rbc