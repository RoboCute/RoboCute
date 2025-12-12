#include "RBCEditorRuntime/MainWindow.h"
// Qt Components
#include <QApplication>
#include <QStatusBar>
#include <QTimer>
// Our Components
#include "RBCEditorRuntime/components/SceneHierarchyWidget.h"
#include "RBCEditorRuntime/components/DetailsPanel.h"
#include "RBCEditorRuntime/components/ResultPanel.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"
#include "RBCEditorRuntime/components/NodeEditor.h"

// Our Runtime
#include "RBCEditorRuntime/animation/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/EditorEngine.h"
#include "RBCEditorRuntime/WorkflowManager.h"
#include "RBCEditorRuntime/EditorContext.h"
#include "RBCEditorRuntime/EditorLayoutManager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      context_(new rbc::EditorContext),
      layoutManager_(nullptr) {
    context_->httpClient = new rbc::HttpClient(this);
    context_->workflowManager = new rbc::WorkflowManager(this);
    // Create editor scene for animation playback
    context_->editorScene = new rbc::EditorScene();

    // Create layout manager
    layoutManager_ = new EditorLayoutManager(this, context_, this);

    // Connect workflow manager signals
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            this, &MainWindow::onWorkflowChanged);
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            layoutManager_, &EditorLayoutManager::onWorkflowChanged);

    // Connect layout manager signals
    connect(layoutManager_, &EditorLayoutManager::workflowSwitchRequested,
            this, &MainWindow::switchWorkflow);
}

MainWindow::~MainWindow() {
    if (context_->sceneSyncManager) {
        context_->sceneSyncManager->stop();
    }
    delete context_;
}

void MainWindow::setupUi() {
    resize(1920, 1080);
    // Set minimum size to ensure proper layout even when no central widget is set
    setMinimumSize(800, 600);
    setWindowTitle("RoboCute Editor");

    // Setup UI through layout manager
    layoutManager_->setupUi();

    // Connect signals for components created by layout manager
    if (context_->sceneHierarchy) {
        connect(context_->sceneHierarchy, &rbc::SceneHierarchyWidget::entitySelected,
                this, &MainWindow::onEntitySelected);
    }

    if (context_->resultPanel) {
        connect(context_->resultPanel, &rbc::ResultPanel::animationSelected,
                this, &MainWindow::onAnimationSelected);
    }

    if (context_->animationPlayer) {
        connect(context_->animationPlayer, &rbc::AnimationPlayer::frameChanged,
                this, &MainWindow::onAnimationFrameChanged);
    }

    // Set default workflow (SceneEditing)
    // switchWorkflow(rbc::WorkflowType::SceneEditing);

    statusBar()->showMessage("Ready");

    // Setup HttpClient in Engine if needed, or engine's one.
    // Ideally, Engine manages networking too, but for now MainWindow owns HttpClient.
    // Let's sync:
    rbc::EditorEngine::instance().setHttpClient(context_->httpClient);
}

void MainWindow::startSceneSync(const QString &serverUrl) {
    // Create scene sync manager if not already created
    if (!context_->sceneSyncManager) {
        context_->sceneSyncManager = new rbc::SceneSyncManager(context_->httpClient, this);
        // Connect signals
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::sceneUpdated,
                this, &MainWindow::onSceneUpdated);
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::connectionStatusChanged,
                this, &MainWindow::onConnectionStatusChanged);
    }
    // Start sync
    context_->sceneSyncManager->start(serverUrl);

    // Trigger node loading in NodeEditor after connection is established
    if (context_->nodeEditor) {
        // Small delay to ensure server connection is ready
        QTimer::singleShot(500, [this]() {
            if (context_->nodeEditor) {
                context_->nodeEditor->loadNodesDeferred();
            }
        });
    }
}

void MainWindow::onSceneUpdated() {
    // Update scene hierarchy
    if (context_->sceneHierarchy && context_->sceneSyncManager) {
        context_->sceneHierarchy->updateFromScene(context_->sceneSyncManager->sceneSync());
    }

    // Update result panel with animations
    if (context_->resultPanel && context_->sceneSyncManager) {
        context_->resultPanel->updateFromSync(context_->sceneSyncManager->sceneSync());
    }
    qDebug() << "Scene updated";
    // update editor scene
    if (context_->editorScene && context_->sceneSyncManager) {
        context_->editorScene->updateFromSync(*context_->sceneSyncManager->sceneSync());
    }
}

void MainWindow::onConnectionStatusChanged(bool connected) {
    if (connected) {
        statusBar()->showMessage("Connected to scene server");
    } else {
        statusBar()->showMessage("Disconnected from scene server");
    }
}

void MainWindow::onEntitySelected(int entityId) {
    // Get entity and resource data
    if (context_->sceneSyncManager) {
        const auto *sceneSync = context_->sceneSyncManager->sceneSync();
        const auto *entity = sceneSync->getEntity(entityId);

        const rbc::EditorResourceMetadata *resource = nullptr;
        if (entity && entity->has_render_component) {
            resource = sceneSync->getResource(entity->render_component.mesh_id);
        }

        // Update details panel
        if (context_->detailsPanel) {
            context_->detailsPanel->showEntity(entity, resource);
        }
    }
}

void MainWindow::onAnimationFrameChanged(int frame) {
    // Update playback manager to apply transforms to scene
    if (context_->playbackManager) {
        context_->playbackManager->setFrame(frame);
    }

    statusBar()->showMessage(QString("Animation frame: %1").arg(frame));
}

void MainWindow::switchWorkflow(rbc::WorkflowType workflow) {
    context_->workflowManager->switchWorkflow(workflow);
}

void MainWindow::onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow) {
    Q_UNUSED(oldWorkflow);

    // Update UI based on workflow
    if (newWorkflow == rbc::WorkflowType::SceneEditing) {
        statusBar()->showMessage("Switched to Scene Editing workflow");
    } else if (newWorkflow == rbc::WorkflowType::Text2Image) {
        statusBar()->showMessage("Switched to Text2Image workflow");
    }
}
