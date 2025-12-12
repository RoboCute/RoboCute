#pragma once

#include <QObject>
#include <QVariant>
#include <QMap>
#include <QList>
#include <QDateTime>
#include <functional>
#include <memory>

#include "RBCEditorRuntime/config.h"

namespace rbc {

/**
 * 事件类型枚举
 */
enum class EventType {
    // 工作流相关
    WorkflowChanged,

    // 场景相关
    SceneUpdated,
    EntitySelected,
    EntityCreated,
    EntityDeleted,
    EntityModified,

    // 动画相关
    AnimationSelected,
    AnimationFrameChanged,
    AnimationPlayStateChanged,
    AnimationPlaybackFinished,

    // 连接相关
    ConnectionStatusChanged,

    // 节点编辑器相关
    NodeDataUpdated,
    NodeExecutionStarted,
    NodeExecutionFinished,

    // 视口相关
    ViewportKeyPressed,
    ViewportMouseClicked,

    // 自定义事件（从1000开始）
    Custom = 1000
};

/**
 * 事件数据结构
 */
struct Event {
    EventType type;
    QVariant data;            // 事件数据，可以是任意类型
    QObject *sender = nullptr;// 事件发送者
    qint64 timestamp = 0;     // 时间戳

    Event(EventType t, const QVariant &d = QVariant(), QObject *s = nullptr)
        : type(t), data(d), sender(s) {
        timestamp = QDateTime::currentMSecsSinceEpoch();
    }
};

/**
 * 事件订阅者接口
 * 组件可以通过继承此接口或使用函数回调来订阅事件
 */
class IEventSubscriber {
public:
    virtual ~IEventSubscriber() = default;
    virtual void onEvent(const Event &event) = 0;
};

/**
 * 事件总线 - 实现发布-订阅模式
 * 
 * 职责：
 * - 作为消息中枢，处理组件间的通信
 * - 实现发布-订阅模式，允许组件间解耦通信
 * - 提供事件过滤和转发机制
 */
class RBC_EDITOR_RUNTIME_API EventBus : public QObject {
    Q_OBJECT
public:
    EventBus(const EventBus &) = delete;
    EventBus &operator=(const EventBus &) = delete;
public:
    /**
     * 获取事件总线单例
     */
    static EventBus &instance();

    /**
     * 订阅事件
     * @param type 事件类型
     * @param subscriber 订阅者对象
     */
    void subscribe(EventType type, IEventSubscriber *subscriber);

    /**
     * 订阅事件（使用函数回调）
     * @param type 事件类型
     * @param callback 回调函数
     * @return 订阅ID，可用于取消订阅
     */
    int subscribe(EventType type, std::function<void(const Event &)> callback);

    /**
     * 取消订阅
     * @param type 事件类型
     * @param subscriber 订阅者对象
     */
    void unsubscribe(EventType type, IEventSubscriber *subscriber);

    /**
     * 取消订阅（使用订阅ID）
     * @param subscriptionId 订阅ID
     */
    void unsubscribe(int subscriptionId);

    /**
     * 发布事件
     * @param event 事件对象
     */
    void publish(const Event &event);

    /**
     * 发布事件（便捷方法）
     * @param type 事件类型
     * @param data 事件数据
     * @param sender 发送者
     */
    void publish(EventType type, const QVariant &data = QVariant(), QObject *sender = nullptr);

    /**
     * 清除所有订阅
     */
    void clear();

signals:
    /**
     * 事件发布信号（用于Qt信号槽机制）
     */
    void eventPublished(const rbc::Event &event);

private:
    explicit EventBus(QObject *parent = nullptr);
    ~EventBus() override = default;

    // 对象订阅者映射：事件类型 -> 订阅者列表
    QMap<EventType, QList<IEventSubscriber *>> objectSubscribers_;

    // 回调订阅者映射：事件类型 -> (订阅ID -> 回调函数)
    QMap<EventType, QMap<int, std::function<void(const Event &)>>> callbackSubscribers_;

    // 订阅ID计数器
    int nextSubscriptionId_ = 1;

    // 订阅ID到事件类型的映射（用于取消订阅）
    QMap<int, EventType> subscriptionIdToType_;
};

}// namespace rbc
