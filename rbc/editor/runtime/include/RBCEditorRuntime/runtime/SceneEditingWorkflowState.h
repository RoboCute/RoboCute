#pragma once

#include "RBCEditorRuntime/runtime/WorkflowState.h"

#include <QWidget>

class QSplitter;

namespace rbc {

/**
 * SceneEditingWorkflowState - 场景编辑工作流状态
 * 
 * 布局：
 * - 中央区域：Viewport（上）+ NodeEditor（下）使用 QSplitter 分割
 * - 左侧：Scene Hierarchy, Connection Status
 * - 右侧：Details, Results + Animation Player
 */
class RBC_EDITOR_RUNTIME_API SceneEditingWorkflowState : public WorkflowState {
    Q_OBJECT

public:
    explicit SceneEditingWorkflowState(QObject* parent = nullptr);
    ~SceneEditingWorkflowState() override;

    void enter(EditorContext* context) override;
    void exit(EditorContext* context) override;
    QWidget* getCentralContainer() override;
    QList<QDockWidget*> getVisibleDocks(EditorContext* context) override;
    QList<QDockWidget*> getHiddenDocks(EditorContext* context) override;
    WorkflowType getType() const override { return WorkflowType::SceneEditing; }
    QString getName() const override { return "Scene Editing"; }

private:
    /**
     * 创建中央容器（如果还未创建）
     */
    void createCentralContainer(EditorContext* context);

    /**
     * 设置 Splitter 的初始比例
     */
    void setupSplitterSizes();

private:
    QWidget* centralContainer_;
    QSplitter* splitter_;
};

}// namespace rbc

