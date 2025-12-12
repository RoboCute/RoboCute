#pragma once
#include <QMainWindow>
#include <QStatusBar>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"
#include "RBCEditorRuntime/EditorContext.h"
#include "RBCEditorRuntime/animation/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/EventBus.h"

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
    void onSceneUpdated();
    void onConnectionStatusChanged(bool connected);
    void onEntitySelected(int entityId);
    void onAnimationFrameChanged(int frame);
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);
    
    /**
     * 处理动画加载完成
     */
    void onAnimationLoaded(const QString &animName);

private:
    /**
     * 订阅事件总线事件
     */
    void subscribeToEventBus();

    /**
     * 处理事件总线事件
     */
    static void onEventBusEvent(const rbc::Event &event);

private:
    rbc::EditorContext *context_;
    EditorLayoutManager *layoutManager_;
    rbc::EventAdapter *eventAdapter_;
    rbc::AnimationController *animationController_;
    int eventBusSubscriptionId_;// 用于取消订阅
};
