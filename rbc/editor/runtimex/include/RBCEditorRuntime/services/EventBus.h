#pragma once

#include "RBCEditorRuntime/services/IEventBus.h"

namespace rbc {

class EventBus : public IEventBus {
    Q_OBJECT
public:
    virtual ~EventBus() = default;
    // delete copy
    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;

public:
    static EventBus &instance();
    // == IEventBus Interface ==
    void publish(const Event &event) override;
    void publish(EventType type, const QVariant &data = QVariant(), QObject *sender = nullptr) override;
    void subscribe(EventType type, std::function<void(const Event &)> handler) override;
    void subscribe(EventType type, IEventSubscriber *subscriber) override;
    void unsubscribe(int subscriptionId) override;
    void unsubscribe(EventType type, IEventSubscriber *subscriber) override;
    void clear() override;
};

}// namespace rbc