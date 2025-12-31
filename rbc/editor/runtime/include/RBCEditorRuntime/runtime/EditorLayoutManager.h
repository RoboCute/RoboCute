#pragma once

#include <QObject>
#include <QActionGroup>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"
#include "RBCEditorRuntime/engine/app_base.h"

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
 * EditorLayoutManager - 负责管理 UI 组件的创建和 DockWidgets 的可见性
 * 
 * 职责（简化版）：
 * - 创建菜单栏(MenuBar)、工具栏(ToolBar)
 * - 创建所有停靠窗口(DockWidgets)
 * - 根据工作流类型控制 DockWidgets 的可见性
 * - 管理渲染模式切换
 * 
 * 不再负责：
 * - CentralWidget 的管理（由 WorkflowContainerManager 负责）
 * - 复杂的布局调整逻辑（由 WorkflowState 负责）
 */
class RBC_EDITOR_RUNTIME_API EditorLayoutManager : public QObject {
    Q_OBJECT

public:
    explicit EditorLayoutManager(QMainWindow *mainWindow, rbc::EditorContext *context, QObject *parent = nullptr);
    ~EditorLayoutManager() override = default;

    /**
     * 初始化所有UI组件（菜单栏、工具栏、Docks）
     */
    void setupUi();

    /**
     * 根据工作流类型调整 DockWidgets 的可见性
     */
    void adjustDockVisibility(rbc::WorkflowType workflow);

public slots:
    /**
     * 响应工作流变化（更新 UI 状态和 Dock 可见性）
     */
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);

signals:
    /**
     * 当需要切换工作流时发出
     */
    void workflowSwitchRequested(rbc::WorkflowType workflow);

    /**
     * 当需要切换渲染模式时发出
     */
    void renderModeSwitchRequested(rbc::RenderMode mode);

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
     * 切换到场景编辑工作流
     */
    void switchToSceneEditingWorkflow();

    /**
     * 切换到Text2Image工作流
     */
    void switchToText2ImageWorkflow();

    /**
     * 切换到编辑器渲染模式
     */
    void switchToEditorRenderMode();

    /**
     * 切换到真实感渲染模式
     */
    void switchToRealisticRenderMode();

    /**
     * 更新渲染模式UI状态
     */
    void updateRenderModeUI(rbc::RenderMode mode);

private:
    QMainWindow *mainWindow_;
    rbc::EditorContext *context_;

    // Workflow actions (menu and toolbar share the same action group)
    QActionGroup *workflowActionGroup_;
    QAction *sceneEditingAction_;
    QAction *text2ImageAction_;
    QAction *sceneEditingToolAction_;
    QAction *text2ImageToolAction_;

    // Render mode actions
    QActionGroup *renderModeActionGroup_;
    QAction *editorModeAction_;
    QAction *realisticModeAction_;
    QAction *editorModeToolAction_;
    QAction *realisticModeToolAction_;
};
