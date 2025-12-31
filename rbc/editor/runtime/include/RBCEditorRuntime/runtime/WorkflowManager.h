#pragma once

#include <QObject>
#include <QWidget>
#include <QDockWidget>
#include <QList>
#include <QMap>

#include "RBCEditorRuntime/config.h"

namespace rbc {

enum class WorkflowType {
    SceneEditing,  // Default workflow with Viewport as central widget
    Text2Image     // Text2Image workflow with NodeGraph as central widget
};

// Forward declarations
class EditorContext;
class WorkflowState;

/**
 * WorkflowManager - 工作流状态机管理器
 * 
 * 负责：
 * - 管理所有 Workflow 状态
 * - 处理状态切换逻辑
 * - 确保切换的原子性和安全性
 * - 发送状态变化通知
 * 
 * 使用状态机模式（State Pattern）：
 * - 每个 Workflow 是一个 WorkflowState 实例
 * - 状态切换通过 enter/exit 方法管理生命周期
 * - 避免直接操作 Widget，委托给具体状态处理
 */
class RBC_EDITOR_RUNTIME_API WorkflowManager : public QObject {
    Q_OBJECT

public:
    explicit WorkflowManager(QObject* parent = nullptr);
    ~WorkflowManager() override;

    /**
     * 初始化所有状态
     * 必须在使用前调用
     */
    void initialize(EditorContext* context);

    /**
     * 获取当前 Workflow 类型
     */
    WorkflowType currentWorkflow() const;

    /**
     * 获取当前状态
     */
    WorkflowState* currentState() const { return currentState_; }

    /**
     * 切换到指定 Workflow
     * 
     * @param workflow 目标 Workflow 类型
     * @return 是否成功切换
     */
    bool switchWorkflow(WorkflowType workflow);

    /**
     * 获取指定类型的状态
     */
    WorkflowState* getState(WorkflowType type) const;

signals:
    /**
     * 即将切换 Workflow（在 exit 之前）
     */
    void workflowWillChange(WorkflowType newWorkflow, WorkflowType oldWorkflow);

    /**
     * Workflow 已切换（在 enter 之后）
     */
    void workflowChanged(WorkflowType newWorkflow, WorkflowType oldWorkflow);

private:
    /**
     * 创建所有状态
     */
    void createStates();

private:
    EditorContext* context_;
    WorkflowState* currentState_;
    QMap<WorkflowType, WorkflowState*> states_;
    
    // 防止重入的标志
    bool isTransitioning_;
};

}// namespace rbc

