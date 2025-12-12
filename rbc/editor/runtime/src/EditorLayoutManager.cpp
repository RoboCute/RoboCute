#include "RBCEditorRuntime/EditorLayoutManager.h"

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

// Our Runtime
#include "RBCEditorRuntime/animation/AnimationPlaybackManager.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/EditorEngine.h"
#include "RBCEditorRuntime/WorkflowManager.h"
#include "RBCEditorRuntime/EditorContext.h"

EditorLayoutManager::EditorLayoutManager(QMainWindow *mainWindow, rbc::EditorContext *context, QObject *parent)
    : QObject(parent),
      mainWindow_(mainWindow),
      context_(context),
      workflowActionGroup_(nullptr),
      sceneEditingAction_(nullptr),
      text2ImageAction_(nullptr),
      sceneEditingToolAction_(nullptr),
      text2ImageToolAction_(nullptr) {
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

    QMenu *workflowMenu = menuBar->addMenu("Workflow");
    // Create shared action group for menu and toolbar
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

    context_->detailsPanel = new rbc::DetailsPanel(detailsDock);
    detailsDock->setWidget(context_->detailsPanel);
    mainWindow_->addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    // 3. Viewport as Dock (can be minimized)
    context_->viewportDock = createViewportDock();
    mainWindow_->addDockWidget(Qt::TopDockWidgetArea, context_->viewportDock);

    // 4. Node Editor (Bottom) - use shared HttpClient
    context_->nodeDock = new QDockWidget("Node Graph", mainWindow_);
    context_->nodeDock->setObjectName("NodeDock");
    context_->nodeDock->setAllowedAreas(Qt::AllDockWidgetAreas);// Allow as central widget
    context_->nodeEditor = new rbc::NodeEditor(context_->httpClient, context_->nodeDock);
    context_->nodeDock->setWidget(context_->nodeEditor);
    mainWindow_->addDockWidget(Qt::BottomDockWidgetArea, context_->nodeDock);

    // 5. Result Panel (Right, below Details)
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

    // Create animation playback manager
    context_->playbackManager = new rbc::AnimationPlaybackManager(context_->editorScene, mainWindow_);
}

QDockWidget *EditorLayoutManager::createViewportDock() {
    context_->viewportWidget = new rbc::ViewportWidget(&rbc::EditorEngine::instance(), mainWindow_);
    context_->viewportWidget->setMinimumSize(400, 300);

    auto *dock = new QDockWidget("Viewport", mainWindow_);
    dock->setObjectName("ViewportDock");
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setWidget(context_->viewportWidget);
    dock->setVisible(true);

    return dock;
}

void EditorLayoutManager::adjustLayoutForWorkflow(rbc::WorkflowType workflow) {
    // 根据工作流类型调整布局
    // SceneEditing: Viewport 作为中央组件（通过 DockWidget）
    // Text2Image: NodeEditor 作为中央组件（通过 DockWidget）
    switch (workflow) {
        case rbc::WorkflowType::SceneEditing:
            // 显示 Viewport DockWidget，隐藏 NodeEditor DockWidget
            if (context_->viewportDock) {
                context_->viewportDock->setVisible(true);
                context_->viewportDock->raise();
            }
            if (context_->nodeDock) {
                context_->nodeDock->setVisible(false);
            }
            // 清除中央组件（使用 DockWidget 布局）
            mainWindow_->setCentralWidget(nullptr);
            break;
        case rbc::WorkflowType::Text2Image:
            // 显示 NodeEditor DockWidget，隐藏 Viewport DockWidget
            if (context_->nodeDock) {
                context_->nodeDock->setVisible(true);
                context_->nodeDock->raise();
            }
            if (context_->viewportDock) {
                context_->viewportDock->setVisible(false);
            }
            // 清除中央组件（使用 DockWidget 布局）
            mainWindow_->setCentralWidget(nullptr);
            break;
    }
}

void EditorLayoutManager::onWorkflowChanged(rbc::WorkflowType newWorkflow, rbc::WorkflowType oldWorkflow) {
    Q_UNUSED(oldWorkflow);

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

    // 调整布局
    adjustLayoutForWorkflow(newWorkflow);
}

void EditorLayoutManager::switchToSceneEditingWorkflow() {
    emit workflowSwitchRequested(rbc::WorkflowType::SceneEditing);
}

void EditorLayoutManager::switchToText2ImageWorkflow() {
    emit workflowSwitchRequested(rbc::WorkflowType::Text2Image);
}
