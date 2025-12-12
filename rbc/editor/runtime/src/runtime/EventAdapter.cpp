#include "RBCEditorRuntime/runtime/EventAdapter.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"

namespace rbc {

EventAdapter::EventAdapter(QObject *parent)
    : QObject(parent) {
}

void EventAdapter::connectSignalsToEventBus() {
    // 这个方法可以在需要时手动调用，或者通过其他方式连接
    // 目前我们通过MainWindow来显式连接
}

void EventAdapter::onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow) {
    QVariantMap data;
    data["newWorkflow"] = static_cast<int>(newWorkflow);
    data["oldWorkflow"] = static_cast<int>(oldWorkflow);

    rbc::EventBus::instance().publish(rbc::EventType::WorkflowChanged, data, sender());
}

void EventAdapter::onEntitySelected(int entityId) {
    rbc::EventBus::instance().publish(rbc::EventType::EntitySelected, entityId, sender());
}

void EventAdapter::onAnimationSelected(const QString &animName) {
    rbc::EventBus::instance().publish(rbc::EventType::AnimationSelected, animName, sender());
}

void EventAdapter::onAnimationFrameChanged(int frame) {
    rbc::EventBus::instance().publish(rbc::EventType::AnimationFrameChanged, frame, sender());
}

void EventAdapter::onSceneUpdated() {
    rbc::EventBus::instance().publish(rbc::EventType::SceneUpdated, QVariant(), sender());
}

void EventAdapter::onConnectionStatusChanged(bool connected) {
    rbc::EventBus::instance().publish(rbc::EventType::ConnectionStatusChanged, connected, sender());
}

}// namespace rbc
