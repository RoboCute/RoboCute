#include "RBCEditorRuntime/services/EventBus.h"

namespace rbc {

void EventBus::publish(const Event &event) {}
void EventBus::publish(EventType type, const QVariant &data, QObject *sender) {}
void EventBus::subscribe(EventType type, std::function<void(const Event &)> handler) {}
void EventBus::subscribe(EventType type, IEventSubscriber *subscriber) {}
void EventBus::unsubscribe(int subscriptionId) {}
void EventBus::unsubscribe(EventType type, IEventSubscriber *subscriber) {}
void EventBus::clear() {}

}// namespace rbc