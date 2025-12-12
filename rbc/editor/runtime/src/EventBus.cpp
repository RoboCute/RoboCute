#include "RBCEditorRuntime/EventBus.h"
#include <QDateTime>
#include <QDebug>

namespace rbc {

EventBus &EventBus::instance() {
    static EventBus bus;
    return bus;
}

EventBus::EventBus(QObject *parent)
    : QObject(parent),
      nextSubscriptionId_(1) {
}

void EventBus::subscribe(EventType type, IEventSubscriber *subscriber) {
    if (!subscriber) {
        qWarning() << "EventBus: Attempted to subscribe with null subscriber";
        return;
    }
    
    objectSubscribers_[type].append(subscriber);
}

int EventBus::subscribe(EventType type, std::function<void(const Event &)> callback) {
    if (!callback) {
        qWarning() << "EventBus: Attempted to subscribe with null callback";
        return -1;
    }
    
    int id = nextSubscriptionId_++;
    callbackSubscribers_[type][id] = callback;
    subscriptionIdToType_[id] = type;
    return id;
}

void EventBus::unsubscribe(EventType type, IEventSubscriber *subscriber) {
    if (!subscriber) {
        return;
    }
    
    auto &subscribers = objectSubscribers_[type];
    subscribers.removeAll(subscriber);
    
    if (subscribers.isEmpty()) {
        objectSubscribers_.remove(type);
    }
}

void EventBus::unsubscribe(int subscriptionId) {
    if (!subscriptionIdToType_.contains(subscriptionId)) {
        return;
    }
    
    EventType type = subscriptionIdToType_[subscriptionId];
    callbackSubscribers_[type].remove(subscriptionId);
    subscriptionIdToType_.remove(subscriptionId);
    
    if (callbackSubscribers_[type].isEmpty()) {
        callbackSubscribers_.remove(type);
    }
}

void EventBus::publish(const Event &event) {
    // 通知对象订阅者
    if (objectSubscribers_.contains(event.type)) {
        const auto &subscribers = objectSubscribers_[event.type];
        for (IEventSubscriber *subscriber : subscribers) {
            if (subscriber) {
                subscriber->onEvent(event);
            }
        }
    }
    
    // 通知回调订阅者
    if (callbackSubscribers_.contains(event.type)) {
        const auto &callbacks = callbackSubscribers_[event.type];
        for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
            if (it.value()) {
                it.value()(event);
            }
        }
    }
    
    // 发出Qt信号（用于信号槽机制）
    emit eventPublished(event);
}

void EventBus::publish(EventType type, const QVariant &data, QObject *sender) {
    Event event(type, data, sender);
    publish(event);
}

void EventBus::clear() {
    objectSubscribers_.clear();
    callbackSubscribers_.clear();
    subscriptionIdToType_.clear();
    nextSubscriptionId_ = 1;
}

}// namespace rbc
