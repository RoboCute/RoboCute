#pragma once

#include <QObject>
#include <QActionGroup>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"

class QMainWindow;
class QMenuBar;
class QToolBar;
class QDockWidget;

namespace rbc {
class EditorContext;
class SceneHierarchyWidget;
class DetailsPanel;
class ViewportWidget;
class ResultPanel;
class AnimationPlayer;
class NodeEditor;
class ConnectionStatusView;
}// namespace rbc

/**
 * EditorLayoutManager - 负责管理所有UI组件的创建、布局和显示/隐藏
 * 
 * 职责：
 * - 管理菜单栏(MenuBar)、工具栏(ToolBar)的创建和配置
 * - 管理所有停靠窗口(DockWidgets)的创建、布局和可见性控制
 * - 根据工作流类型动态调整UI布局
 */
class RBC_EDITOR_RUNTIME_API EditorLayoutManager : public QObject {
    Q_OBJECT

public:
    explicit EditorLayoutManager(QMainWindow *mainWindow, rbc::EditorContext *context, QObject *parent = nullptr);
    ~EditorLayoutManager() override = default;

    /**
     * 初始化所有UI组件
     */
    void setupUi();

    /**
     * 根据工作流类型调整UI布局
     */
    void adjustLayoutForWorkflow(rbc::WorkflowType workflow);

public slots:
    /**
     * 响应工作流变化
     */
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);

signals:
    /**
     * 当需要切换工作流时发出
     */
    void workflowSwitchRequested(rbc::WorkflowType workflow);

private:
    /**
     * 创建菜单栏
     */
    void setupMenuBar();

    /**
     * 创建工具栏
     */
    void setupToolBar();

    /**
     * 创建所有停靠窗口
     */
    void setupDocks();

    /**
     * 创建视口停靠窗口
     */
    QDockWidget *createViewportDock();

    /**
     * 切换到场景编辑工作流
     */
    void switchToSceneEditingWorkflow();

    /**
     * 切换到Text2Image工作流
     */
    void switchToText2ImageWorkflow();

private:
    QMainWindow *mainWindow_;
    rbc::EditorContext *context_;

    // Workflow actions (menu and toolbar share the same action group)
    QActionGroup *workflowActionGroup_;
    QAction *sceneEditingAction_;
    QAction *text2ImageAction_;
    QAction *sceneEditingToolAction_;
    QAction *text2ImageToolAction_;
};
