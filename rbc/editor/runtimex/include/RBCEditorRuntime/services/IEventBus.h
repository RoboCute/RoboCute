#pragma once
#include <QObject>
#include <QVariant>
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"

// 事件总线

namespace rbc {

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

class IEventBus : public QObject {
    Q_OBJECT
public:
    virtual ~IEventBus() = default;

    // publish event
    virtual void publish(const Event &event) = 0;
    virtual void publish(EventType type, const QVariant &data = QVariant()) = 0;

    // subscibe event
    virtual void subscribe(EventType type, std::function<void(const Event &)> handler) = 0;
    virtual void subscribe(QObject *receiver, EventType type, const char *slot) = 0;

    // cancel subscription
    virtual void unsubscribe(EventType type, std::function<void(const Event &)> handler) = 0;
    virtual void unsubscribe(QObject *receiver) = 0;

signals:
    void event_published(const Event &event);
};

}// namespace rbc