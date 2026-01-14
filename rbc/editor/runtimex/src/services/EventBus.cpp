#include "RBCEditorRuntime/services/EventBus.h"

namespace rbc {

// 重要：有 Q_OBJECT 宏的类必须在 .cpp 文件中定义析构器
EventBus::~EventBus() {
    // 空实现，但必须在 .cpp 文件中定义
    // Qt 会在这里正确清理元对象和信号槽系统
}

void EventBus::publish(const Event &event) {}
void EventBus::publish(EventType type, const QVariant &data, QObject *sender) {}
void EventBus::subscribe(EventType type, std::function<void(const Event &)> handler) {}
void EventBus::subscribe(EventType type, IEventSubscriber *subscriber) {}
void EventBus::unsubscribe(int subscriptionId) {}
void EventBus::unsubscribe(EventType type, IEventSubscriber *subscriber) {}
void EventBus::clear() {}

}// namespace rbc