#pragma once

#include <QObject>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/EventBus.h"

namespace rbc {

class EditorContext;
class SceneSyncManager;

/**
 * SceneUpdater - 处理场景更新相关的逻辑
 * 
 * 职责：
 * - 响应场景变化事件，更新场景层次结构和相关视图
 * - 更新结果面板等依赖场景数据的组件
 */
class RBC_EDITOR_RUNTIME_API SceneUpdater : public QObject, public IEventSubscriber {
    Q_OBJECT

public:
    explicit SceneUpdater(EditorContext *context, QObject *parent = nullptr);
    ~SceneUpdater() override;

    /**
     * 初始化更新器，订阅相关事件
     */
    void initialize();

    /**
     * 处理场景更新
     */
    void handleSceneUpdated();

    /**
     * IEventSubscriber 接口实现
     */
    void onEvent(const Event &event) override;

signals:
    /**
     * 当场景更新完成时发出
     */
    void sceneUpdateCompleted();

private:
    EditorContext *context_;
    int sceneUpdatedSubscriptionId_;
};

}// namespace rbc
