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
#include "RBCEditorRuntime/runtime/EditorScene.h"
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

    // 3. Viewport - will be set as central widget in SceneEditing mode
    context_->viewportDock = createViewportDock();
    // Don't add as dock initially, will be set as central widget

    // 4. Node Editor (Bottom) - use shared HttpClient
    context_->nodeDock = new QDockWidget("Node Graph", mainWindow_);
    context_->nodeDock->setObjectName("NodeDock");
    context_->nodeDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    context_->nodeEditor = new rbc::NodeEditor(context_->httpClient, context_->nodeDock);
    context_->nodeDock->setWidget(context_->nodeEditor);
    // Don't add as dock initially, will be added in adjustLayoutForWorkflow

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

    // 6. Connection Status View (Left, below Scene Hierarchy)
    context_->connectionStatusDock = new QDockWidget("Connection Status", mainWindow_);
    context_->connectionStatusDock->setObjectName("ConnectionStatusDock");
    context_->connectionStatusDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    
    context_->connectionStatusView = new rbc::ConnectionStatusView(context_->connectionStatusDock);
    context_->connectionStatusDock->setWidget(context_->connectionStatusView);
    mainWindow_->addDockWidget(Qt::LeftDockWidgetArea, context_->connectionStatusDock);
    
    // Stack Connection Status dock below Scene Hierarchy dock
    mainWindow_->splitDockWidget(sceneDock, context_->connectionStatusDock, Qt::Vertical);

    // Create animation playback manager
    context_->playbackManager = new rbc::AnimationPlaybackManager(context_->editorScene, mainWindow_);

    // Set default layout for SceneEditing workflow
    adjustLayoutForWorkflow(rbc::WorkflowType::SceneEditing);
}

QDockWidget *EditorLayoutManager::createViewportDock() {
    context_->viewportWidget = new rbc::ViewportWidget(&rbc::EditorEngine::instance(), mainWindow_);
    context_->viewportWidget->setMinimumSize(400, 300);
    // Set EditorContext for drag and drop support
    context_->viewportWidget->setEditorContext(context_);

    auto *dock = new QDockWidget("Viewport", mainWindow_);
    dock->setObjectName("ViewportDock");
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setWidget(context_->viewportWidget);
    dock->setVisible(true);

    return dock;
}

void EditorLayoutManager::adjustLayoutForWorkflow(rbc::WorkflowType workflow) {
    // 根据工作流类型调整布局
    switch (workflow) {
        case rbc::WorkflowType::SceneEditing:
            // SceneEditing: Viewport 作为中央组件，NodeEditor 在底部
            // 首先移除所有dock（如果已添加）
            if (context_->viewportDock && context_->viewportDock->parent() == mainWindow_) {
                mainWindow_->removeDockWidget(context_->viewportDock);
            }
            if (context_->nodeDock && context_->nodeDock->parent() == mainWindow_) {
                mainWindow_->removeDockWidget(context_->nodeDock);
            }

            // 如果ViewportWidget还在dock中，需要先取出
            if (context_->viewportDock && context_->viewportDock->widget() == context_->viewportWidget) {
                context_->viewportDock->setWidget(nullptr);
            }

            // 将 Viewport 设置为中央组件，占据主要空间
            if (context_->viewportWidget) {
                // 显式设置parent为mainWindow（如果还没有设置）
                // 这是关键修复：从dock中取出widget后，parent可能仍然是dock，
                // 需要显式设置为mainWindow才能正确显示
                if (context_->viewportWidget->parentWidget() != mainWindow_) {
                    context_->viewportWidget->setParent(mainWindow_);
                }
                
                mainWindow_->setCentralWidget(context_->viewportWidget);
                
                // 确保ViewportWidget可见并更新
                context_->viewportWidget->show();
                context_->viewportWidget->setVisible(true);
                context_->viewportWidget->update();
                context_->viewportWidget->repaint();
            }

            // 将 NodeEditor 添加到底部，设置合理的高度
            if (context_->nodeDock) {
                mainWindow_->addDockWidget(Qt::BottomDockWidgetArea, context_->nodeDock);
                context_->nodeDock->setVisible(true);
                // 设置NodeEditor的最小高度，确保有足够的空间
                context_->nodeDock->setMinimumHeight(300);
                // 使用窗口高度的30-35%作为NodeEditor的高度，确保Viewport有足够空间
                // 计算可用高度（窗口高度减去菜单栏、工具栏、状态栏等）
                int availableHeight = mainWindow_->height();
                if (mainWindow_->menuBar()) {
                    availableHeight -= mainWindow_->menuBar()->height();
                }
                if (mainWindow_->statusBar()) {
                    availableHeight -= mainWindow_->statusBar()->height();
                }
                // 使用30%的高度给NodeEditor，剩余70%给Viewport
                int nodeEditorHeight = qMax(300, static_cast<int>(availableHeight * 0.3));
                // 使用resizeDocks来分配空间
                QList<QDockWidget*> docks;
                docks << context_->nodeDock;
                QList<int> sizes;
                sizes << nodeEditorHeight;
                mainWindow_->resizeDocks(docks, sizes, Qt::Vertical);
            }
            break;
        case rbc::WorkflowType::Text2Image:
            // Text2Image: NodeEditor 作为中央组件，隐藏 Viewport
            // 移除Viewport（如果作为central widget）
            if (mainWindow_->centralWidget() == context_->viewportWidget) {
                mainWindow_->setCentralWidget(nullptr);
                // 将ViewportWidget放回dock
                if (context_->viewportDock && context_->viewportWidget) {
                    context_->viewportDock->setWidget(context_->viewportWidget);
                }
            }
            // 移除dock（如果已添加）
            if (context_->viewportDock && context_->viewportDock->parent() == mainWindow_) {
                mainWindow_->removeDockWidget(context_->viewportDock);
                context_->viewportDock->setVisible(false);
            }
            if (context_->nodeDock && context_->nodeDock->parent() == mainWindow_) {
                mainWindow_->removeDockWidget(context_->nodeDock);
            }

            // 如果NodeEditor还在dock中，需要先取出
            if (context_->nodeDock && context_->nodeDock->widget() == context_->nodeEditor) {
                context_->nodeDock->setWidget(nullptr);
            }

            // 将 NodeEditor 设置为中央组件
            if (context_->nodeEditor) {
                mainWindow_->setCentralWidget(context_->nodeEditor);
            }
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
