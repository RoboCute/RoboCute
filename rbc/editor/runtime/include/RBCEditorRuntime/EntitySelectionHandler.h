#pragma once

#include <QObject>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/EventBus.h"

namespace rbc {

class EditorContext;
class SceneSyncManager;

/**
 * EntitySelectionHandler - 处理实体选择相关的逻辑
 * 
 * 职责：
 * - 响应实体选择事件，更新属性面板和相关视图
 * - 处理实体相关的上下文操作
 */
class RBC_EDITOR_RUNTIME_API EntitySelectionHandler : public QObject, public IEventSubscriber {
    Q_OBJECT

public:
    explicit EntitySelectionHandler(EditorContext *context, QObject *parent = nullptr);
    ~EntitySelectionHandler() override;

    /**
     * 初始化处理器，订阅相关事件
     */
    void initialize();

    /**
     * 处理实体选择
     * @param entityId 实体ID
     */
    void handleEntitySelected(int entityId);

    /**
     * IEventSubscriber 接口实现
     */
    void onEvent(const Event &event) override;

signals:
    /**
     * 当实体选择完成时发出
     */
    void entitySelectionCompleted(int entityId);

private:
    EditorContext *context_;
    int entitySelectedSubscriptionId_;
};

}// namespace rbc
