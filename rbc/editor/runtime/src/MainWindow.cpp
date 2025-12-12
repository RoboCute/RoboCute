#include "RBCEditorRuntime/MainWindow.h"
// Qt Components
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QApplication>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QActionGroup>
// Our Components
#include "RBCEditorRuntime/components/NodeEditor.h"
#include "RBCEditorRuntime/components/SceneHierarchyWidget.h"
#include "RBCEditorRuntime/components/DetailsPanel.h"
#include "RBCEditorRuntime/components/ViewportWidget.h"
#include "RBCEditorRuntime/components/ResultPanel.h"
#include "RBCEditorRuntime/components/AnimationPlayer.h"

// Our Runtime
#include "RBCEditorRuntime/animation/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/EditorEngine.h"
#include "RBCEditorRuntime/WorkflowManager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      httpClient_(new rbc::HttpClient(this)),
      sceneSyncManager_(nullptr),
      workflowManager_(new rbc::WorkflowManager(this)),
      sceneHierarchy_(nullptr),
      detailsPanel_(nullptr),
      viewportWidget_(nullptr),
      viewportDock_(nullptr),
      resultPanel_(nullptr),
      animationPlayer_(nullptr),
      playbackManager_(nullptr),
      editorScene_(nullptr),
      nodeEditor_(nullptr),
      nodeDock_(nullptr),
      workflowActionGroup_(nullptr),
      sceneEditingAction_(nullptr),
      text2ImageAction_(nullptr),
      sceneEditingToolAction_(nullptr),
      text2ImageToolAction_(nullptr) {
    // Create editor scene for animation playback
    editorScene_ = new rbc::EditorScene();

    // Connect workflow manager signals
    connect(workflowManager_, &rbc::WorkflowManager::workflowChanged,
            this, &MainWindow::onWorkflowChanged);
}

MainWindow::~MainWindow() {
    if (sceneSyncManager_) {
        sceneSyncManager_->stop();
    }
}

void MainWindow::setupUi() {
    resize(1920, 1080);
    // Set minimum size to ensure proper layout even when no central widget is set
    setMinimumSize(800, 600);
    setWindowTitle("RoboCute Editor");

    setupMenuBar();
    setupToolBar();
    setupDocks();

    // Set default workflow (SceneEditing)
    // switchWorkflow(rbc::WorkflowType::SceneEditing);

    statusBar()->showMessage("Ready");

    // Setup HttpClient in Engine if needed, or engine's one.
    // Ideally, Engine manages networking too, but for now MainWindow owns HttpClient.
    // Let's sync:
    rbc::EditorEngine::instance().setHttpClient(httpClient_);
}

void MainWindow::startSceneSync(const QString &serverUrl) {
    // Create scene sync manager if not already created
    if (!sceneSyncManager_) {
        sceneSyncManager_ = new rbc::SceneSyncManager(httpClient_, this);
        // Connect signals
        connect(sceneSyncManager_, &rbc::SceneSyncManager::sceneUpdated,
                this, &MainWindow::onSceneUpdated);
        connect(sceneSyncManager_, &rbc::SceneSyncManager::connectionStatusChanged,
                this, &MainWindow::onConnectionStatusChanged);
    }
    // Start sync
    sceneSyncManager_->start(serverUrl);

    // Trigger node loading in NodeEditor after connection is established
    if (nodeEditor_) {
        // Small delay to ensure server connection is ready
        QTimer::singleShot(500, [this]() {
            if (nodeEditor_) {
                nodeEditor_->loadNodesDeferred();
            }
        });
    }
}

void MainWindow::onSceneUpdated() {
    // Update scene hierarchy
    if (sceneHierarchy_ && sceneSyncManager_) {
        sceneHierarchy_->updateFromScene(sceneSyncManager_->sceneSync());
    }

    // Update result panel with animations
    if (resultPanel_ && sceneSyncManager_) {
        resultPanel_->updateFromSync(sceneSyncManager_->sceneSync());
    }
    qDebug() << "Scene updated";
    // update editor scene
    if (editorScene_ && sceneSyncManager_) {
        editorScene_->updateFromSync(*sceneSyncManager_->sceneSync());
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
    if (sceneSyncManager_) {
        const auto *sceneSync = sceneSyncManager_->sceneSync();
        const auto *entity = sceneSync->getEntity(entityId);

        const rbc::EditorResourceMetadata *resource = nullptr;
        if (entity && entity->has_render_component) {
            resource = sceneSync->getResource(entity->render_component.mesh_id);
        }

        // Update details panel
        if (detailsPanel_) {
            detailsPanel_->showEntity(entity, resource);
        }
    }
}

void MainWindow::onAnimationFrameChanged(int frame) {
    // Update playback manager to apply transforms to scene
    if (playbackManager_) {
        playbackManager_->setFrame(frame);
    }

    statusBar()->showMessage(QString("Animation frame: %1").arg(frame));
}

void MainWindow::switchWorkflow(rbc::WorkflowType workflow) {
    workflowManager_->switchWorkflow(workflow);
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

void MainWindow::switchToSceneEditingWorkflow() {
    switchWorkflow(rbc::WorkflowType::SceneEditing);
}

void MainWindow::switchToText2ImageWorkflow() {
    switchWorkflow(rbc::WorkflowType::Text2Image);
}

void MainWindow::setupMenuBar() {

    QMenu *fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("New Scene");
    fileMenu->addAction("Open Scene...");
    fileMenu->addSeparator();
    fileMenu->addAction("Save");
    fileMenu->addAction("Save As...");
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", qApp, &QApplication::quit);

    QMenu *editMenu = menuBar()->addMenu("Edit");
    editMenu->addAction("Undo");
    editMenu->addAction("Redo");
    editMenu->addSeparator();
    editMenu->addAction("Preferences...");

    QMenu *workflowMenu = menuBar()->addMenu("Workflow");
    // Create shared action group for menu and toolbar
    workflowActionGroup_ = new QActionGroup(this);

    sceneEditingAction_ = workflowMenu->addAction("Scene Editing");
    sceneEditingAction_->setCheckable(true);
    sceneEditingAction_->setChecked(true);
    sceneEditingAction_->setActionGroup(workflowActionGroup_);
    connect(sceneEditingAction_, &QAction::triggered, this, &MainWindow::switchToSceneEditingWorkflow);

    text2ImageAction_ = workflowMenu->addAction("Text2Image");
    text2ImageAction_->setCheckable(true);
    text2ImageAction_->setActionGroup(workflowActionGroup_);
    connect(text2ImageAction_, &QAction::triggered, this, &MainWindow::switchToText2ImageWorkflow);

    QMenu *windowMenu = menuBar()->addMenu("Window");
    // Actions to toggle docks could go here

    QMenu *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("About");
}

void MainWindow::setupToolBar() {
    QToolBar *toolbar = addToolBar("Main Tools");
    toolbar->setObjectName("MainToolbar");
    toolbar->setMovable(false);

    // Workflow switcher (use shared action group from menu)
    sceneEditingToolAction_ = toolbar->addAction("Scene");
    sceneEditingToolAction_->setCheckable(true);
    sceneEditingToolAction_->setChecked(true);
    sceneEditingToolAction_->setActionGroup(workflowActionGroup_);
    connect(sceneEditingToolAction_, &QAction::triggered, this, &MainWindow::switchToSceneEditingWorkflow);

    text2ImageToolAction_ = toolbar->addAction("Text2Image");
    text2ImageToolAction_->setCheckable(true);
    text2ImageToolAction_->setActionGroup(workflowActionGroup_);
    connect(text2ImageToolAction_, &QAction::triggered, this, &MainWindow::switchToText2ImageWorkflow);

    toolbar->addSeparator();

    toolbar->addAction("Select");
    toolbar->addAction("Translate");
    toolbar->addAction("Rotate");
    toolbar->addAction("Scale");
    toolbar->addSeparator();
    toolbar->addAction("Play");
    toolbar->addAction("Pause");
}

void MainWindow::setupDocks() {
    // 1. Scene Hierarchy (Left)
    auto *sceneDock = new QDockWidget("Scene Hierarchy", this);
    sceneDock->setObjectName("SceneDock");
    sceneDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    sceneHierarchy_ = new rbc::SceneHierarchyWidget(sceneDock);
    sceneDock->setWidget(sceneHierarchy_);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    // Connect entity selection signal
    connect(sceneHierarchy_, &rbc::SceneHierarchyWidget::entitySelected,
            this, &MainWindow::onEntitySelected);

    // 2. Details Panel (Right)
    auto *detailsDock = new QDockWidget("Details", this);
    detailsDock->setObjectName("DetailsDock");
    detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    detailsPanel_ = new rbc::DetailsPanel(detailsDock);
    detailsDock->setWidget(detailsPanel_);
    addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    // 3. Viewport as Dock (can be minimized)
    viewportDock_ = createViewportDock();
    addDockWidget(Qt::TopDockWidgetArea, viewportDock_);

    // 4. Node Editor (Bottom) - use shared HttpClient
    nodeDock_ = new QDockWidget("Node Graph", this);
    nodeDock_->setObjectName("NodeDock");
    nodeDock_->setAllowedAreas(Qt::AllDockWidgetAreas);// Allow as central widget
    nodeEditor_ = new rbc::NodeEditor(httpClient_, nodeDock_);
    nodeDock_->setWidget(nodeEditor_);
    addDockWidget(Qt::BottomDockWidgetArea, nodeDock_);

    // 5. Result Panel (Right, below Details)
    auto *resultDock = new QDockWidget("Results", this);
    resultDock->setObjectName("ResultDock");
    resultDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // Create a container widget for ResultPanel and AnimationPlayer
    auto *resultContainer = new QWidget(resultDock);
    auto *resultLayout = new QVBoxLayout(resultContainer);

    resultPanel_ = new rbc::ResultPanel(resultContainer);
    animationPlayer_ = new rbc::AnimationPlayer(resultContainer);

    resultLayout->addWidget(resultPanel_);
    resultLayout->addWidget(animationPlayer_);
    resultContainer->setLayout(resultLayout);

    resultDock->setWidget(resultContainer);
    addDockWidget(Qt::RightDockWidgetArea, resultDock);

    // Stack Result dock below Details dock
    splitDockWidget(detailsDock, resultDock, Qt::Vertical);

    // Create animation playback manager
    playbackManager_ = new rbc::AnimationPlaybackManager(editorScene_, this);

    // Connect signals
    connect(resultPanel_, &rbc::ResultPanel::animationSelected,
            this, &MainWindow::onAnimationSelected);
    connect(animationPlayer_, &rbc::AnimationPlayer::frameChanged,
            this, &MainWindow::onAnimationFrameChanged);
}

QDockWidget *MainWindow::createViewportDock() {

    viewportWidget_ = new rbc::ViewportWidget(&rbc::EditorEngine::instance(), this);
    viewportWidget_->setMinimumSize(400, 300);

    auto *dock = new QDockWidget("Viewport", this);
    dock->setObjectName("ViewportDock");
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setWidget(viewportWidget_);
    dock->setVisible(true);

    return dock;
}
