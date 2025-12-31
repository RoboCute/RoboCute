#include "RBCEditorRuntime/runtime/EditorLayoutManager.h"

// Qt Components
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
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
#include "RBCEditorRuntime/components/ConnectionStatusView.h"

// Our Runtime
#include "RBCEditorRuntime/runtime/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/engine/EditorEngine.h"
#include "RBCEditorRuntime/runtime/WorkflowManager.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"

EditorLayoutManager::EditorLayoutManager(QMainWindow *mainWindow, rbc::EditorContext *context, QObject *parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      context_(context),
      workflowActionGroup_(nullptr),
      sceneEditingAction_(nullptr),
      text2ImageAction_(nullptr),
      sceneEditingToolAction_(nullptr),
      text2ImageToolAction_(nullptr),
      renderModeActionGroup_(nullptr),
      editorModeAction_(nullptr),
      realisticModeAction_(nullptr),
      editorModeToolAction_(nullptr),
      realisticModeToolAction_(nullptr) {
    Q_ASSERT(mainWindow_ != nullptr);
    Q_ASSERT(context_ != nullptr);
}

void EditorLayoutManager::setupUi() {
    setupMenuBar();
    setupToolBar();
    setupDocks();
}

void EditorLayoutManager::setupMenuBar() {
    QMenuBar *menuBar = mainWindow_->menuBar();

    QMenu *fileMenu = menuBar->addMenu("File");
    fileMenu->addAction("New Scene");
    fileMenu->addAction("Open Scene...");
    fileMenu->addSeparator();
    fileMenu->addAction("Save");
    fileMenu->addAction("Save As...");
    fileMenu->addSeparator();
    fileMenu->addAction("Exit", qApp, &QApplication::quit);

    QMenu *editMenu = menuBar->addMenu("Edit");
    editMenu->addAction("Undo");
    editMenu->addAction("Redo");
    editMenu->addSeparator();
    editMenu->addAction("Preferences...");

    // Workflow menu
    QMenu *workflowMenu = menuBar->addMenu("Workflow");
    workflowActionGroup_ = new QActionGroup(this);

    sceneEditingAction_ = workflowMenu->addAction("Scene Editing");
    sceneEditingAction_->setCheckable(true);
    sceneEditingAction_->setChecked(true);
    sceneEditingAction_->setActionGroup(workflowActionGroup_);
    connect(sceneEditingAction_, &QAction::triggered, this, &EditorLayoutManager::switchToSceneEditingWorkflow);

    text2ImageAction_ = workflowMenu->addAction("Text2Image");
    text2ImageAction_->setCheckable(true);
    text2ImageAction_->setActionGroup(workflowActionGroup_);
    connect(text2ImageAction_, &QAction::triggered, this, &EditorLayoutManager::switchToText2ImageWorkflow);

    // View menu with render mode options
    QMenu *viewMenu = menuBar->addMenu("View");
    QMenu *renderModeMenu = viewMenu->addMenu("Render Mode");
    renderModeActionGroup_ = new QActionGroup(this);

    editorModeAction_ = renderModeMenu->addAction("Editor Preview");
    editorModeAction_->setCheckable(true);
    editorModeAction_->setChecked(true);
    editorModeAction_->setActionGroup(renderModeActionGroup_);
    editorModeAction_->setToolTip("Fast rasterized preview with selection and interaction support");
    connect(editorModeAction_, &QAction::triggered, this, &EditorLayoutManager::switchToEditorRenderMode);

    realisticModeAction_ = renderModeMenu->addAction("Realistic Render");
    realisticModeAction_->setCheckable(true);
    realisticModeAction_->setActionGroup(renderModeActionGroup_);
    realisticModeAction_->setToolTip("Path traced realistic rendering with PBR materials");
    connect(realisticModeAction_, &QAction::triggered, this, &EditorLayoutManager::switchToRealisticRenderMode);

    QMenu *windowMenu = menuBar->addMenu("Window");
    // Actions to toggle docks could go here

    QMenu *helpMenu = menuBar->addMenu("Help");
    helpMenu->addAction("About");
}

void EditorLayoutManager::setupToolBar() {
    QToolBar *toolbar = mainWindow_->addToolBar("Main Tools");
    toolbar->setObjectName("MainToolbar");
    toolbar->setMovable(false);

    // Workflow switcher (use shared action group from menu)
    sceneEditingToolAction_ = toolbar->addAction("Scene");
    sceneEditingToolAction_->setCheckable(true);
    sceneEditingToolAction_->setChecked(true);
    sceneEditingToolAction_->setActionGroup(workflowActionGroup_);
    connect(sceneEditingToolAction_, &QAction::triggered, this, &EditorLayoutManager::switchToSceneEditingWorkflow);

    text2ImageToolAction_ = toolbar->addAction("Text2Image");
    text2ImageToolAction_->setCheckable(true);
    text2ImageToolAction_->setActionGroup(workflowActionGroup_);
    connect(text2ImageToolAction_, &QAction::triggered, this, &EditorLayoutManager::switchToText2ImageWorkflow);

    toolbar->addSeparator();

    // Render mode switcher
    editorModeToolAction_ = toolbar->addAction("Editor");
    editorModeToolAction_->setCheckable(true);
    editorModeToolAction_->setChecked(true);
    editorModeToolAction_->setActionGroup(renderModeActionGroup_);
    editorModeToolAction_->setToolTip("Editor Preview Mode - Fast rasterized preview with interaction");
    connect(editorModeToolAction_, &QAction::triggered, this, &EditorLayoutManager::switchToEditorRenderMode);

    realisticModeToolAction_ = toolbar->addAction("Realistic");
    realisticModeToolAction_->setCheckable(true);
    realisticModeToolAction_->setActionGroup(renderModeActionGroup_);
    realisticModeToolAction_->setToolTip("Realistic Render Mode - Path traced PBR rendering");
    connect(realisticModeToolAction_, &QAction::triggered, this, &EditorLayoutManager::switchToRealisticRenderMode);

    toolbar->addSeparator();

    toolbar->addAction("Select");
    toolbar->addAction("Translate");
    toolbar->addAction("Rotate");
    toolbar->addAction("Scale");
    toolbar->addSeparator();
    toolbar->addAction("Play");
    toolbar->addAction("Pause");
}

void EditorLayoutManager::setupDocks() {
    // 1. Scene Hierarchy (Left)
    auto *sceneDock = new QDockWidget("Scene Hierarchy", mainWindow_);
    sceneDock->setObjectName("SceneDock");
    sceneDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    context_->sceneHierarchy = new rbc::SceneHierarchyWidget(sceneDock);
    sceneDock->setWidget(context_->sceneHierarchy);
    mainWindow_->addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    // 2. Details Panel (Right)
    auto *detailsDock = new QDockWidget("Details", mainWindow_);
    detailsDock->setObjectName("DetailsDock");
    detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    context_->detailsPanel = new rbc::DetailsPanel(context_->httpClient, context_, detailsDock);
    detailsDock->setWidget(context_->detailsPanel);
    mainWindow_->addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    // 3. Result Panel (Right, below Details)
    auto *resultDock = new QDockWidget("Results", mainWindow_);
    resultDock->setObjectName("ResultDock");
    resultDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    // Create a container widget for ResultPanel and AnimationPlayer
    auto *resultContainer = new QWidget(resultDock);
    auto *resultLayout = new QVBoxLayout(resultContainer);

    context_->resultPanel = new rbc::ResultPanel(resultContainer);
    context_->animationPlayer = new rbc::AnimationPlayer(resultContainer);

    resultLayout->addWidget(context_->resultPanel);
    resultLayout->addWidget(context_->animationPlayer);
    resultContainer->setLayout(resultLayout);

    resultDock->setWidget(resultContainer);
    mainWindow_->addDockWidget(Qt::RightDockWidgetArea, resultDock);

    // Stack Result dock below Details dock
    mainWindow_->splitDockWidget(detailsDock, resultDock, Qt::Vertical);

    // 4. Connection Status View (Left, below Scene Hierarchy)
    context_->connectionStatusDock = new QDockWidget("Connection Status", mainWindow_);
    context_->connectionStatusDock->setObjectName("ConnectionStatusDock");
    context_->connectionStatusDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    context_->connectionStatusView = new rbc::ConnectionStatusView(context_->connectionStatusDock);
    context_->connectionStatusDock->setWidget(context_->connectionStatusView);
    mainWindow_->addDockWidget(Qt::LeftDockWidgetArea, context_->connectionStatusDock);
    
    // Stack Connection Status dock below Scene Hierarchy dock
    mainWindow_->splitDockWidget(sceneDock, context_->connectionStatusDock, Qt::Vertical);

    // 5. Create Viewport Widget (will be used in workflow containers)
    context_->viewportWidget = new rbc::ViewportWidget(&rbc::EditorEngine::instance(), mainWindow_);
    context_->viewportWidget->setMinimumSize(400, 300);
    context_->viewportWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    context_->viewportWidget->setEditorContext(context_);
    // 初始时隐藏，等待 WorkflowState 管理其显示
    context_->viewportWidget->setVisible(false);
    // Note: ViewportWidget 不再放在 dock 中，而是由 WorkflowState 管理

    // 6. Create Node Editor (will be used in workflow containers)
    context_->nodeEditor = new rbc::NodeEditor(context_->httpClient, mainWindow_);
    context_->nodeEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // 初始时隐藏，等待 WorkflowState 管理其显示
    context_->nodeEditor->setVisible(false);
    // Note: NodeEditor 不再放在 dock 中，而是由 WorkflowState 管理

    // Create animation playback manager
    context_->playbackManager = new rbc::AnimationPlaybackManager(context_->editorScene, mainWindow_);

    qDebug() << "EditorLayoutManager: Setup docks completed";
}

void EditorLayoutManager::adjustDockVisibility(rbc::WorkflowType workflow) {
    // 根据工作流类型调整 DockWidgets 的可见性
    // CentralWidget 的管理已经由 WorkflowContainerManager 和 WorkflowState 负责
    // 这里只需要控制 Dock 的可见性
    
    qDebug() << "EditorLayoutManager: Adjusting dock visibility for workflow" 
             << static_cast<int>(workflow);

    switch (workflow) {
        case rbc::WorkflowType::SceneEditing:
            // SceneEditing: 所有 Dock 都可见
            // （Scene Hierarchy, Details, Results, Connection Status）
            // 由 WorkflowState 决定具体哪些 Dock 可见
            break;
        case rbc::WorkflowType::Text2Image:
            // Text2Image: 只显示 Results 和 Connection Status
            // 隐藏 Scene Hierarchy, Details
            // 由 WorkflowState 决定具体哪些 Dock 可见
            break;
    }
}

void EditorLayoutManager::onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow) {
    Q_UNUSED(oldWorkflow);

    qDebug() << "EditorLayoutManager: onWorkflowChanged from" << static_cast<int>(oldWorkflow)
             << "to" << static_cast<int>(newWorkflow);

    // 更新工作流动作的选中状态
    switch (newWorkflow) {
        case rbc::WorkflowType::SceneEditing:
            if (sceneEditingAction_) {
                sceneEditingAction_->setChecked(true);
            }
            if (sceneEditingToolAction_) {
                sceneEditingToolAction_->setChecked(true);
            }
            break;
        case rbc::WorkflowType::Text2Image:
            if (text2ImageAction_) {
                text2ImageAction_->setChecked(true);
            }
            if (text2ImageToolAction_) {
                text2ImageToolAction_->setChecked(true);
            }
            break;
    }

    // 调整 Dock 可见性
    adjustDockVisibility(newWorkflow);
}

void EditorLayoutManager::switchToSceneEditingWorkflow() {
    emit workflowSwitchRequested(rbc::WorkflowType::SceneEditing);
}

void EditorLayoutManager::switchToText2ImageWorkflow() {
    emit workflowSwitchRequested(rbc::WorkflowType::Text2Image);
}

void EditorLayoutManager::switchToEditorRenderMode() {
    rbc::EditorEngine::instance().setRenderMode(rbc::RenderMode::Editor);
    updateRenderModeUI(rbc::RenderMode::Editor);
    emit renderModeSwitchRequested(rbc::RenderMode::Editor);
}

void EditorLayoutManager::switchToRealisticRenderMode() {
    rbc::EditorEngine::instance().setRenderMode(rbc::RenderMode::Realistic);
    updateRenderModeUI(rbc::RenderMode::Realistic);
    emit renderModeSwitchRequested(rbc::RenderMode::Realistic);
}

void EditorLayoutManager::updateRenderModeUI(rbc::RenderMode mode) {
    switch (mode) {
        case rbc::RenderMode::Editor:
            if (editorModeAction_) {
                editorModeAction_->setChecked(true);
            }
            if (editorModeToolAction_) {
                editorModeToolAction_->setChecked(true);
            }
            break;
        case rbc::RenderMode::Realistic:
            if (realisticModeAction_) {
                realisticModeAction_->setChecked(true);
            }
            if (realisticModeToolAction_) {
                realisticModeToolAction_->setChecked(true);
            }
            break;
    }
}
