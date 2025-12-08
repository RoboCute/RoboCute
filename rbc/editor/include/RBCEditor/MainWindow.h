#pragma once
#include <QMainWindow>
#include "RBCEditor/WorkflowManager.h"

namespace rbc {
class HttpClient;
class SceneSyncManager;
class SceneHierarchyWidget;
class DetailsPanel;
class ViewportWidget;
class ResultPanel;
class AnimationPlayer;
class AnimationPlaybackManager;
class EditorScene;
class NodeEditor;
class WorkflowManager;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setupUi();
    ~MainWindow();

    // Access to HTTP client for scene sync
    rbc::HttpClient *httpClient() { return httpClient_; }
    rbc::SceneSyncManager *sceneSyncManager() { return sceneSyncManager_; }

    // Start scene synchronization
    void startSceneSync(const QString &serverUrl = "http://127.0.0.1:5555");

private:
    void setupMenuBar();
    void setupToolBar();
    void setupDocks();
    void switchWorkflow(rbc::WorkflowType workflow);
    QDockWidget* createViewportDock();

private slots:
    void onSceneUpdated();
    void onConnectionStatusChanged(bool connected);
    void onEntitySelected(int entityId);
    void onAnimationSelected(QString animName);
    void onAnimationFrameChanged(int frame);
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);
    void switchToSceneEditingWorkflow();
    void switchToText2ImageWorkflow();

private:
    rbc::HttpClient *httpClient_;
    rbc::SceneSyncManager *sceneSyncManager_;
    rbc::WorkflowManager *workflowManager_;
    rbc::SceneHierarchyWidget *sceneHierarchy_;
    rbc::DetailsPanel *detailsPanel_;
    rbc::ViewportWidget *viewportWidget_;
    QDockWidget *viewportDock_;
    rbc::ResultPanel *resultPanel_;
    rbc::AnimationPlayer *animationPlayer_;
    rbc::AnimationPlaybackManager *playbackManager_;
    rbc::EditorScene *editorScene_;
    rbc::NodeEditor *nodeEditor_;
    QDockWidget *nodeDock_;
    
    // Workflow actions (menu and toolbar share the same action group)
    QActionGroup *workflowActionGroup_;
    QAction *sceneEditingAction_;
    QAction *text2ImageAction_;
    QAction *sceneEditingToolAction_;
    QAction *text2ImageToolAction_;
};
