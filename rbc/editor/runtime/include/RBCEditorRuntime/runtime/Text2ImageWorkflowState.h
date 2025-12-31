#pragma once

#include "RBCEditorRuntime/runtime/WorkflowState.h"

#include <QWidget>

namespace rbc {

/**
 * Text2ImageWorkflowState - Text2Image 工作流状态
 * 
 * 布局：
 * - 中央区域：NodeEditor（全屏）
 * - 左侧：Connection Status（可选）
 * - 右侧：Results + Animation Player
 * - 隐藏：Viewport, Scene Hierarchy, Details
 */
class RBC_EDITOR_RUNTIME_API Text2ImageWorkflowState : public WorkflowState {
    Q_OBJECT

public:
    explicit Text2ImageWorkflowState(QObject* parent = nullptr);
    ~Text2ImageWorkflowState() override;

    void enter(EditorContext* context) override;
    void exit(EditorContext* context) override;
    QWidget* getCentralContainer() override;
    QList<QDockWidget*> getVisibleDocks(EditorContext* context) override;
    QList<QDockWidget*> getHiddenDocks(EditorContext* context) override;
    WorkflowType getType() const override { return WorkflowType::Text2Image; }
    QString getName() const override { return "Text2Image"; }

private:
    /**
     * 创建中央容器（如果还未创建）
     */
    void createCentralContainer(EditorContext* context);

private:
    QWidget* centralContainer_;
};

}// namespace rbc

