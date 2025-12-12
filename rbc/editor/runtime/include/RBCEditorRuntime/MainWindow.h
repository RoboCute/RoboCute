#pragma once
#include <QMainWindow>
#include <QStatusBar>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/runtime/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/EventBus.h"

namespace rbc {
class HttpClient;
class SceneHierarchyWidget;
class DetailsPanel;
class ViewportWidget;
class ResultPanel;
class EditorScene;
class NodeEditor;
class WorkflowManager;
class EventAdapter;
class AnimationController;
class SceneUpdater;
class EntitySelectionHandler;
}// namespace rbc

class EditorLayoutManager;

class RBC_EDITOR_RUNTIME_API MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setupUi();
    ~MainWindow();

    // Access to HTTP client for scene sync
    rbc::HttpClient *httpClient() { return context_->httpClient; }
    rbc::SceneSyncManager *sceneSyncManager() { return context_->sceneSyncManager; }

    // Start scene synchronization
    void startSceneSync(const QString &serverUrl = "http://127.0.0.1:5555");

private:
    void switchWorkflow(rbc::WorkflowType workflow);

private slots:
    /**
     * UI 更新相关的槽函数（仅用于状态栏等 UI 更新）
     */
    void onConnectionStatusChanged(bool connected);
    void onAnimationFrameChanged(int frame);
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);

    /**
     * 处理动画加载完成（UI 更新）
     */
    void onAnimationLoaded(const QString &animName);

private:
    /**
     * 订阅事件总线事件（目前主要用于 UI 更新相关的事件）
     */
    void subscribeToEventBus();

private:
    rbc::EditorContext *context_;
    EditorLayoutManager *layoutManager_;
    rbc::EventAdapter *eventAdapter_;
    rbc::AnimationController *animationController_;
    rbc::SceneUpdater *sceneUpdater_;
    rbc::EntitySelectionHandler *entitySelectionHandler_;
    int eventBusSubscriptionId_;// 用于取消订阅
};
