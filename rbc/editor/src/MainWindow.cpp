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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
}

MainWindow::~MainWindow() {}

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

    auto *sceneTree = new QTreeWidget(sceneDock);
    sceneTree->setHeaderHidden(true);

    auto *root = new QTreeWidgetItem(sceneTree, {"Scene Root"});
    root->setExpanded(true);
    new QTreeWidgetItem(root, {"Main Camera"});
    new QTreeWidgetItem(root, {"Directional Light"});
    auto *models = new QTreeWidgetItem(root, {"Models"});
    models->setExpanded(true);
    new QTreeWidgetItem(models, {"Robot Arm Base"});
    new QTreeWidgetItem(models, {"Robot Arm Link 1"});

    sceneDock->setWidget(sceneTree);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    // 2. Details Panel (Right)
    QDockWidget *detailsDock = new QDockWidget("Details", this);
    detailsDock->setObjectName("DetailsDock");
    detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QListWidget *detailsList = new QListWidget(detailsDock);
    // Mocking properties
    detailsList->addItem("Transform");
    detailsList->addItem("   Position: [0.0, 0.0, 0.0]");
    detailsList->addItem("   Rotation: [0.0, 0.0, 0.0]");
    detailsList->addItem("   Scale:    [1.0, 1.0, 1.0]");
    detailsList->addItem("Mesh Renderer");
    detailsList->addItem("   Material: Default");
    detailsList->addItem("   Cast Shadows: On");

    detailsDock->setWidget(detailsList);
    addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    // 3. Node Editor (Bottom)
    auto *nodeDock = new QDockWidget("Node Graph", this);
    nodeDock->setObjectName("NodeDock");
    nodeDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    auto *node_editor = new rbc::NodeEditor(nodeDock);
    nodeDock->setWidget(node_editor);
    addDockWidget(Qt::BottomDockWidgetArea, nodeDock);
}
