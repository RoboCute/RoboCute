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
#include "RBCEditorRuntime/components/ConnectionStatusView.h"

// Our Runtime
#include "RBCEditorRuntime/engine/EditorEngine.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include "RBCEditorRuntime/runtime/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/WorkflowState.h"
#include "RBCEditorRuntime/runtime/WorkflowContainerManager.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/runtime/EditorLayoutManager.h"
#include "RBCEditorRuntime/runtime/EventBus.h"
#include "RBCEditorRuntime/runtime/EventAdapter.h"
#include "RBCEditorRuntime/runtime/AnimationController.h"
#include "RBCEditorRuntime/runtime/SceneUpdater.h"
#include "RBCEditorRuntime/runtime/EntitySelectionHandler.h"
#include "RBCEditorRuntime/runtime/ViewportSelectionSync.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      context_(new rbc::EditorContext),
      layoutManager_(nullptr),
      containerManager_(nullptr),
      eventAdapter_(nullptr),
      animationController_(nullptr),
      sceneUpdater_(nullptr),
      entitySelectionHandler_(nullptr),
      viewportSelectionSync_(nullptr),
      eventBusSubscriptionId_(-1) {

    context_->httpClient = new rbc::HttpClient(this);
    context_->workflowManager = new rbc::WorkflowManager(this);
    context_->editorScene = new rbc::EditorScene();

    // Register EditorScene with EditorEngine for frame tick processing
    rbc::EditorEngine::instance().setEditorScene(context_->editorScene);

    layoutManager_ = new EditorLayoutManager(this, context_, this);
    containerManager_ = new rbc::WorkflowContainerManager(this, context_, this);
    eventAdapter_ = new rbc::EventAdapter(this);
    animationController_ = new rbc::AnimationController(context_, this);
    animationController_->initialize();
    sceneUpdater_ = new rbc::SceneUpdater(context_, this);
    sceneUpdater_->initialize();
    entitySelectionHandler_ = new rbc::EntitySelectionHandler(context_, this);
    entitySelectionHandler_->initialize();
    viewportSelectionSync_ = new ViewportSelectionSync(context_, this);
    viewportSelectionSync_->initialize();

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

    // Connect workflow manager to container manager for central widget switching
    connect(context_->workflowManager, &rbc::WorkflowManager::workflowChanged,
            this, &MainWindow::onWorkflowChangedForContainer);

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
    resize(1920, 1200);
    // Set minimum size to ensure proper layout even when no central widget is set
    setMinimumSize(800, 600);
    setWindowTitle("RoboCute Editor");

    // 1. Setup UI components (Docks, MenuBar, ToolBar) through layout manager
    layoutManager_->setupUi();

    // 2. Initialize container manager (creates StackedWidget as CentralWidget)
    containerManager_->initialize();

    // 3. Initialize workflow manager and create all workflow states
    context_->workflowManager->initialize(context_);

    // 4. Register all workflow containers
    registerWorkflowContainers();

    // 5. Switch to default workflow (SceneEditing)
    // This will trigger the workflow state machine and set up the initial layout
    switchWorkflow(rbc::WorkflowType::SceneEditing);

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

    // Setup ConnectionStatusView
    if (context_->connectionStatusView) {
        context_->connectionStatusView->setHttpClient(context_->httpClient);
        // Connect reconnect request signal
        connect(context_->connectionStatusView, &rbc::ConnectionStatusView::reconnectRequested,
                this, &MainWindow::startSceneSync);
        // Update initial connection status if SceneSyncManager already exists
        if (context_->sceneSyncManager) {
            context_->connectionStatusView->setSceneSyncManager(context_->sceneSyncManager);
            context_->connectionStatusView->updateConnectionStatus(context_->sceneSyncManager->isConnected());
            // Connect connection status changes to update the view
            connect(context_->sceneSyncManager, &rbc::SceneSyncManager::connectionStatusChanged,
                    context_->connectionStatusView, &rbc::ConnectionStatusView::updateConnectionStatus);
        }
    }

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

        // Connect to ConnectionStatusView if it exists
        if (context_->connectionStatusView) {
            context_->connectionStatusView->setSceneSyncManager(context_->sceneSyncManager);
            connect(context_->sceneSyncManager, &rbc::SceneSyncManager::connectionStatusChanged,
                    context_->connectionStatusView, &rbc::ConnectionStatusView::updateConnectionStatus);
        }
    } else {
        // If already exists, stop and restart with new URL
        context_->sceneSyncManager->stop();
    }
    // Start sync
    context_->sceneSyncManager->start(serverUrl);

    // Update ConnectionStatusView URL display if it exists
    if (context_->connectionStatusView) {
        context_->connectionStatusView->updateServerUrl(serverUrl);
    }

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

void MainWindow::onWorkflowChangedForContainer(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow) {
    Q_UNUSED(oldWorkflow);

    // Get the current state (which should have just entered)
    auto* state = context_->workflowManager->getState(newWorkflow);
    if (!state) {
        qWarning() << "MainWindow: No state found for workflow" << static_cast<int>(newWorkflow);
        return;
    }

    // Get the container (should be created by enter())
    QWidget* container = state->getCentralContainer();
    if (!container) {
        qWarning() << "MainWindow: Container not created for workflow" << static_cast<int>(newWorkflow);
        return;
    }

    // Register the container if not already registered
    // This handles the case where the container was just created by enter()
    if (!containerManager_->stackedWidget() || 
        containerManager_->stackedWidget()->indexOf(container) == -1) {
        containerManager_->registerContainer(newWorkflow, container);
    }

    // Switch to the container
    containerManager_->switchContainer(newWorkflow);
}

void MainWindow::registerWorkflowContainers() {
    // Register containers for all workflow states
    // Note: Containers are created lazily in enter(), so we register them
    // when they are created during workflow switching (in onWorkflowChangedForContainer)
    // This function is kept for potential future use or explicit pre-registration
    // For now, containers will be registered automatically when workflows are entered
}
