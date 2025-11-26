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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    resize(1600, 900);
    setWindowTitle("RoboCute Editor");

    // Central Widget (3D Viewport Placeholder)
    QLabel *centralWidget = new QLabel("3D Viewport", this);
    centralWidget->setAlignment(Qt::AlignCenter);
    centralWidget->setObjectName("Viewport");// For styling
    setCentralWidget(centralWidget);

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
    QDockWidget *sceneDock = new QDockWidget("Scene Hierarchy", this);
    sceneDock->setObjectName("SceneDock");
    sceneDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QTreeWidget *sceneTree = new QTreeWidget(sceneDock);
    sceneTree->setHeaderHidden(true);

    QTreeWidgetItem *root = new QTreeWidgetItem(sceneTree, {"Scene Root"});
    root->setExpanded(true);
    new QTreeWidgetItem(root, {"Main Camera"});
    new QTreeWidgetItem(root, {"Directional Light"});
    QTreeWidgetItem *models = new QTreeWidgetItem(root, {"Models"});
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
    QDockWidget *nodeDock = new QDockWidget("Node Graph", this);
    nodeDock->setObjectName("NodeDock");
    nodeDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);

    QLabel *nodePlaceholder = new QLabel("Node Graph Editor Placeholder", nodeDock);
    nodePlaceholder->setAlignment(Qt::AlignCenter);
    nodePlaceholder->setObjectName("NodeEditorPlaceholder");

    nodeDock->setWidget(nodePlaceholder);
    addDockWidget(Qt::BottomDockWidgetArea, nodeDock);
}
