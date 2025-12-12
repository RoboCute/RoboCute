#pragma once
#include <QMainWindow>
#include <QStatusBar>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"
#include "RBCEditorRuntime/EditorContext.h"
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

    inline void onAnimationSelected(const QString &animName) {
        if (!context_->sceneSyncManager) return;

        const auto *sceneSync = context_->sceneSyncManager->sceneSync();
        luisa::string anim_name{animName.toStdString()};
        const auto *anim = sceneSync->getAnimation(anim_name);

        if (anim && context_->animationPlayer && context_->playbackManager) {
            context_->animationPlayer->setAnimation(animName, anim->total_frames, anim->fps);
            context_->playbackManager->setAnimation(anim);
            statusBar()->showMessage(QString("Loaded animation: %1").arg(animName));
        }
    }

    void onAnimationFrameChanged(int frame);
    void onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow);

private:
    rbc::EditorContext *context_;
    EditorLayoutManager *layoutManager_;
};
