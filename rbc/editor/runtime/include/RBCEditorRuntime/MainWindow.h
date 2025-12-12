#pragma once
#include <QMainWindow>
#include <QStatusBar>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"
#include "RBCEditorRuntime/animation/AnimationPlaybackManager.h"

namespace rbc {
class HttpClient;
class SceneHierarchyWidget;
class DetailsPanel;
class ViewportWidget;
class ResultPanel;
class EditorScene;
class NodeEditor;
class WorkflowManager;
}// namespace rbc

class RBC_EDITOR_RUNTIME_API MainWindow : public QMainWindow {
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
    QDockWidget *createViewportDock();

private slots:
    void onSceneUpdated();
    void onConnectionStatusChanged(bool connected);
    void onEntitySelected(int entityId);

    inline void onAnimationSelected(const QString &animName) {
        if (!sceneSyncManager_) return;

        const auto *sceneSync = sceneSyncManager_->sceneSync();
        luisa::string anim_name{animName.toStdString()};
        const auto *anim = sceneSync->getAnimation(anim_name);

        if (anim && animationPlayer_ && playbackManager_) {
            animationPlayer_->setAnimation(animName, anim->total_frames, anim->fps);
            playbackManager_->setAnimation(anim);
            statusBar()->showMessage(QString("Loaded animation: %1").arg(animName));
        }
    }

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
