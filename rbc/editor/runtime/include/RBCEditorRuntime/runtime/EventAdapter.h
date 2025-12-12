#pragma once

#include <QObject>
#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/EventBus.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"

namespace rbc {

/**
 * 事件适配器 - 将Qt信号槽机制桥接到事件总线
 * 
 * 这个类帮助将现有的信号槽机制逐步迁移到事件总线系统
 */
class RBC_EDITOR_RUNTIME_API EventAdapter : public QObject {
    Q_OBJECT

public:
    explicit EventAdapter(QObject *parent = nullptr);
    ~EventAdapter() override = default;

    /**
     * 连接信号到事件总线
     * 将Qt信号转换为事件总线事件
     */
    void connectSignalsToEventBus();

public slots:
    /**
     * 处理工作流变化信号
     */
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);

    /**
     * 处理实体选择信号
     */
    void onEntitySelected(int entityId);

    /**
     * 处理动画选择信号
     */
    void onAnimationSelected(const QString &animName);

    /**
     * 处理动画帧变化信号
     */
    void onAnimationFrameChanged(int frame);

    /**
     * 处理场景更新信号
     */
    void onSceneUpdated();

    /**
     * 处理连接状态变化信号
     */
    void onConnectionStatusChanged(bool connected);
};

}// namespace rbc
