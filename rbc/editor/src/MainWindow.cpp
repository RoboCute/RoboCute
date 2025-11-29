#include "RBCEditor/MainWindow.h"
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QLabel>
#include <QKeyEvent>
#include <QApplication>
#include <QStatusBar>
#include "RBCEditor/components/NodeEditor.h"
#include "RBCEditor/components/SceneHierarchyWidget.h"
#include "RBCEditor/components/DetailsPanel.h"
#include "RBCEditor/runtime/HttpClient.h"
#include "RBCEditor/runtime/SceneSyncManager.h"

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent),
      httpClient_(new rbc::HttpClient(this)),
      sceneSyncManager_(nullptr),
      sceneHierarchy_(nullptr),
      detailsPanel_(nullptr) {
}

MainWindow::~MainWindow() {
    if (sceneSyncManager_) {
        sceneSyncManager_->stop();
    }
}

void MainWindow::setupUi(QWidget *central_viewport) {
    resize(1600, 900);
    setWindowTitle("RoboCute Editor");
    // 3D Viewport
    setCentralWidget(central_viewport);

    setupMenuBar();
    setupToolBar();
    setupDocks();

    statusBar()->showMessage("Ready");
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
}

void MainWindow::onSceneUpdated() {
    // Update scene hierarchy
    if (sceneHierarchy_ && sceneSyncManager_) {
        sceneHierarchy_->updateFromScene(sceneSyncManager_->sceneSync());
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

    QMenu *windowMenu = menuBar()->addMenu("Window");
    // Actions to toggle docks could go here

    QMenu *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("About");
}

void MainWindow::setupToolBar() {
    QToolBar *toolbar = addToolBar("Main Tools");
    toolbar->setObjectName("MainToolbar");
    toolbar->setMovable(false);

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
    QDockWidget *detailsDock = new QDockWidget("Details", this);
    detailsDock->setObjectName("DetailsDock");
    detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    detailsPanel_ = new rbc::DetailsPanel(detailsDock);
    detailsDock->setWidget(detailsPanel_);
    addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    // 3. Node Editor (Bottom)
    auto *nodeDock = new QDockWidget("Node Graph", this);
    nodeDock->setObjectName("NodeDock");
    nodeDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    auto *node_editor = new rbc::NodeEditor(nodeDock);
    nodeDock->setWidget(node_editor);
    addDockWidget(Qt::BottomDockWidgetArea, nodeDock);
}
