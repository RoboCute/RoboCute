#pragma once
#include <QObject>
#include <QVariant>
#include "RBCEditorRuntime/infra/events/Event.h"
#include "RBCEditorRuntime/infra/events/EventType.h"
#include "RBCEditorRuntime/services/IService.h"

// Event bus

namespace rbc {

class IEventSubscriber {
public:
    virtual ~IEventSubscriber() = default;
    virtual void onEvent(const Event &event) = 0;
};

/**
 * Use Case
 * ================

// Usage example
void MyViewModel::init() {
    // Lambda subscription
    eventBus_->subscribe(EventType::EntitySelected,
        [this](const Event& e) {
            int entityId = e.data.toInt();
            this->onEntitySelected(entityId);
        });

    // Signal-slot subscription
    eventBus_->subscribe(this, EventType::SceneUpdated,
        SLOT(onSceneUpdated()));
}

void MyViewModel::selectEntity(int id) {
    // Publish event
    eventBus_->publish(EventType::EntitySelected, id);
}

*/

/**
 * IEventBus - Event bus service interface
 * 
 * Important: As an interface class, use an inline destructor to avoid link conflicts.
 * Concrete implementations (e.g., EventBus) should define the destructor in a .cpp file.
 */
class IEventBus : public IService {
    Q_OBJECT
public:
    explicit IEventBus(QObject *parent = nullptr) : IService(parent) {}
    ~IEventBus() override = default;// Inline default implementation to avoid conflicts with moc-generated code

    // IService interface
    [[nodiscard]] QString serviceId() const override { return "com.robocute.event_bus"; }

    // publish event
    virtual void publish(const Event &event) = 0;
    virtual void publish(EventType type, const QVariant &data = QVariant(), QObject *sender = nullptr) = 0;

    // subscribe event (callback & subscriber)
    virtual void subscribe(EventType type, std::function<void(const Event &)> handler) = 0;
    virtual void subscribe(EventType type, IEventSubscriber *subscriber) = 0;

    // cancel subscription
    virtual void unsubscribe(int subscriptionId) = 0;
    virtual void unsubscribe(EventType type, IEventSubscriber *subscriber) = 0;

    // clear all subscriptions
    virtual void clear() = 0;

signals:
    void eventPublished(const Event &event);
};

}// namespace rbc