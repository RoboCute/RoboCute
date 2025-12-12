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
#include "RBCEditorRuntime/engine/EditorEngine.h"
#include "RBCEditorRuntime/WorkflowManager.h"
#include "RBCEditorRuntime/EditorContext.h"
#include "RBCEditorRuntime/EditorLayoutManager.h"
#include "RBCEditorRuntime/EventBus.h"
#include "RBCEditorRuntime/EventAdapter.h"
#include "RBCEditorRuntime/CommandBus.h"
#include "RBCEditorRuntime/AnimationController.h"
#include "RBCEditorRuntime/SceneUpdater.h"
#include "RBCEditorRuntime/EntitySelectionHandler.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      context_(new rbc::EditorContext),
      layoutManager_(nullptr),
      eventAdapter_(nullptr),
      animationController_(nullptr),
      sceneUpdater_(nullptr),
      entitySelectionHandler_(nullptr),
      eventBusSubscriptionId_(-1) {
    context_->httpClient = new rbc::HttpClient(this);
    context_->workflowManager = new rbc::WorkflowManager(this);
    // Create editor scene for animation playback
    context_->editorScene = new rbc::EditorScene();

    // Create layout manager
    layoutManager_ = new EditorLayoutManager(this, context_, this);

    // Create event adapter
    eventAdapter_ = new rbc::EventAdapter(this);

    // Create animation controller
    animationController_ = new rbc::AnimationController(context_, this);
    animationController_->initialize();

    // Create scene updater
    sceneUpdater_ = new rbc::SceneUpdater(context_, this);
    sceneUpdater_->initialize();

    // Create entity selection handler
    entitySelectionHandler_ = new rbc::EntitySelectionHandler(context_, this);
    entitySelectionHandler_->initialize();

    // Connect animation controller signals (for UI updates)
    connect(animationController_, &rbc::AnimationController::animationLoaded,
            this, &MainWindow::onAnimationLoaded);

    // Connect workflow manager signals (both to slots and event bus)
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            this, &MainWindow::onWorkflowChanged);
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            layoutManager_, &EditorLayoutManager::onWorkflowChanged);
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            eventAdapter_, &rbc::EventAdapter::onWorkflowChanged);

    // Connect layout manager signals
    connect(layoutManager_, &EditorLayoutManager::workflowSwitchRequested,
            this, &MainWindow::switchWorkflow);

    // Subscribe to event bus
    subscribeToEventBus();
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
    // All business logic now goes through event bus via EventAdapter
    if (context_->sceneHierarchy) {
        // 实体选择通过事件总线处理，不再直接连接到 MainWindow
        connect(context_->sceneHierarchy, &rbc::SceneHierarchyWidget::entitySelected,
                eventAdapter_, &rbc::EventAdapter::onEntitySelected);
    }

    if (context_->resultPanel) {
        // 动画选择通过事件总线处理，不再直接连接到 MainWindow
        connect(context_->resultPanel, &rbc::ResultPanel::animationSelected,
                eventAdapter_, &rbc::EventAdapter::onAnimationSelected);
    }

    if (context_->animationPlayer) {
        // 动画帧变化通过事件总线处理
        connect(context_->animationPlayer, &rbc::AnimationPlayer::frameChanged,
                eventAdapter_, &rbc::EventAdapter::onAnimationFrameChanged);
        // 同时保留到 MainWindow 的连接以更新状态栏
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
        // Connect signals to event bus via adapter
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::sceneUpdated,
                eventAdapter_, &rbc::EventAdapter::onSceneUpdated);
        // Connection status changes still go to MainWindow for UI updates
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::connectionStatusChanged,
                this, &MainWindow::onConnectionStatusChanged);
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::connectionStatusChanged,
                eventAdapter_, &rbc::EventAdapter::onConnectionStatusChanged);
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

void MainWindow::onConnectionStatusChanged(bool connected) {
    if (connected) {
        statusBar()->showMessage("Connected to scene server");
    } else {
        statusBar()->showMessage("Disconnected from scene server");
    }
}

void MainWindow::onAnimationFrameChanged(int frame) {
    // 动画帧变化现在通过事件总线处理
    // AnimationController 会处理实际的逻辑
    // 这里只更新状态栏显示
    statusBar()->showMessage(QString("Animation frame: %1").arg(frame));
}

void MainWindow::onAnimationLoaded(const QString &animName) {
    // 动画加载完成，更新状态栏
    statusBar()->showMessage(QString("Loaded animation: %1").arg(animName));
}

void MainWindow::switchWorkflow(rbc::WorkflowType workflow) {
    context_->workflowManager->switchWorkflow(workflow);
}

void MainWindow::subscribeToEventBus() {
    // MainWindow 现在主要通过专门的 Manager 组件处理事件
    // 这里只订阅需要 UI 更新的事件（如工作流变化）
    eventBusSubscriptionId_ = rbc::EventBus::instance().subscribe(
        rbc::EventType::WorkflowChanged,
        [this](const rbc::Event &event) {
            // 工作流变化由 WorkflowManager 和 EditorLayoutManager 处理
            // MainWindow 只负责 UI 更新（状态栏）
        });
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
