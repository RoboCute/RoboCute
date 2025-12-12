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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      context_(new rbc::EditorContext),
      layoutManager_(nullptr),
      eventAdapter_(nullptr),
      animationController_(nullptr),
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

    // Connect animation controller signals
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
    // Also connect to event adapter for event bus integration
    if (context_->sceneHierarchy) {
        connect(context_->sceneHierarchy, &rbc::SceneHierarchyWidget::entitySelected,
                this, &MainWindow::onEntitySelected);
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
        // Connect signals (both to slots and event bus)
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::sceneUpdated,
                this, &MainWindow::onSceneUpdated);
        connect(context_->sceneSyncManager, &rbc::SceneSyncManager::sceneUpdated,
                eventAdapter_, &rbc::EventAdapter::onSceneUpdated);
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
    // Subscribe to event bus events using callback
    eventBusSubscriptionId_ = rbc::EventBus::instance().subscribe(
        rbc::EventType::WorkflowChanged,
        [this](const rbc::Event &event) {
            onEventBusEvent(event);
        });

    // Also subscribe to other important events
    rbc::EventBus::instance().subscribe(
        rbc::EventType::EntitySelected,
        [this](const rbc::Event &event) {
            onEventBusEvent(event);
        });

    rbc::EventBus::instance().subscribe(
        rbc::EventType::SceneUpdated,
        [this](const rbc::Event &event) {
            onEventBusEvent(event);
        });

    rbc::EventBus::instance().subscribe(
        rbc::EventType::AnimationFrameChanged,
        [this](const rbc::Event &event) {
            onEventBusEvent(event);
        });

    rbc::EventBus::instance().subscribe(
        rbc::EventType::ConnectionStatusChanged,
        [this](const rbc::Event &event) {
            onEventBusEvent(event);
        });
}

void MainWindow::onEventBusEvent(const rbc::Event &event) {
    // 处理事件总线事件
    // 这里可以实现基于事件的业务逻辑
    // 目前保留原有的信号槽处理，事件总线作为补充
    switch (event.type) {
        case rbc::EventType::WorkflowChanged:
            // 可以通过事件总线统一处理工作流变化
            break;
        case rbc::EventType::EntitySelected:
            // 可以通过事件总线统一处理实体选择
            break;
        case rbc::EventType::SceneUpdated:
            // 可以通过事件总线统一处理场景更新
            break;
        default:
            break;
    }
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
