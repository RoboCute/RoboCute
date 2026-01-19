#pragma once
#include <QObject>
#include <QVariant>
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"
#include "RBCEditorRuntime/services/IService.h"

// 事件总线

namespace rbc {

class IEventSubscriber {
public:
    virtual ~IEventSubscriber() = default;
    virtual void onEvent(const Event &event) = 0;
};

/**
 * Use Case
 * ================

// 使用示例
void MyViewModel::init() {
    // Lambda订阅
    eventBus_->subscribe(EventType::EntitySelected, 
        [this](const Event& e) {
            int entityId = e.data.toInt();
            this->onEntitySelected(entityId);
        });
    
    // 信号槽订阅
    eventBus_->subscribe(this, EventType::SceneUpdated, 
        SLOT(onSceneUpdated()));
}

void MyViewModel::selectEntity(int id) {
    // 发布事件
    eventBus_->publish(EventType::EntitySelected, id);
}

*/

/**
 * IEventBus - Event bus service interface
 * 
 * 重要：作为接口类，使用内联析构器避免链接冲突
 * 具体实现类（如 EventBus）应在 .cpp 中定义析构器
 */
class IEventBus : public IService {
    Q_OBJECT
public:
    explicit IEventBus(QObject *parent = nullptr) : IService(parent) {}
    ~IEventBus() override {}// 内联空实现，避免与 moc 生成的代码冲突

    // IService interface
    QString serviceId() const override { return "com.robocute.event_bus"; }

    // publish event
    virtual void publish(const Event &event) = 0;
    virtual void publish(EventType type, const QVariant &data = QVariant(), QObject *sender = nullptr) = 0;

    // subscibe event (callback & subscriber)
    virtual void subscribe(EventType type, std::function<void(const Event &)> handler) = 0;
    virtual void subscribe(EventType type, IEventSubscriber *subscriber) = 0;

    // cancel subscription
    virtual void unsubscribe(int subscriptionId) = 0;
    virtual void unsubscribe(EventType type, IEventSubscriber *subscriber) = 0;

    // clear all subscrib
    virtual void clear() = 0;

signals:
    void eventPublished(const Event &event);
};

}// namespace rbc