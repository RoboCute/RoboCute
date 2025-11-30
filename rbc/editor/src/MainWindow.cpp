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
#include <QTimer>
#include <QVBoxLayout>
#include "RBCEditor/components/NodeEditor.h"
#include "RBCEditor/components/SceneHierarchyWidget.h"
#include "RBCEditor/components/DetailsPanel.h"
#include "RBCEditor/components/ViewportWidget.h"
#include "RBCEditor/components/ResultPanel.h"
#include "RBCEditor/components/AnimationPlayer.h"
#include "RBCEditor/animation/AnimationPlaybackManager.h"
#include "RBCEditor/runtime/HttpClient.h"
#include "RBCEditor/runtime/SceneSyncManager.h"
#include "RBCEditor/runtime/EditorScene.h"
#include "RBCEditor/EditorEngine.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      httpClient_(new rbc::HttpClient(this)),
      sceneSyncManager_(nullptr),
      sceneHierarchy_(nullptr),
      detailsPanel_(nullptr),
      viewportWidget_(nullptr),
      resultPanel_(nullptr),
      animationPlayer_(nullptr),
      playbackManager_(nullptr),
      editorScene_(nullptr),
      nodeEditor_(nullptr) {
    // Create editor scene for animation playback
    editorScene_ = new rbc::EditorScene();
}

MainWindow::~MainWindow() {
    if (sceneSyncManager_) {
        sceneSyncManager_->stop();
    }
}

void MainWindow::setupUi() {
    resize(1600, 900);
    setWindowTitle("RoboCute Editor");

    // 3D Viewport
    viewportWidget_ = new rbc::ViewportWidget(&rbc::EditorEngine::instance(), this);
    setCentralWidget(viewportWidget_);

    setupMenuBar();
    setupToolBar();
    setupDocks();

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

void MainWindow::onAnimationSelected(QString animName) {
    if (!sceneSyncManager_) return;

    const auto *sceneSync = sceneSyncManager_->sceneSync();
    const auto *anim = sceneSync->getAnimation(animName.toStdString().c_str());

    if (anim && animationPlayer_ && playbackManager_) {
        // Set animation in animation player UI
        animationPlayer_->setAnimation(animName, anim->total_frames, anim->fps);

        // Set animation in playback manager
        playbackManager_->setAnimation(anim);

        statusBar()->showMessage(QString("Loaded animation: %1").arg(animName));
    }
}

void MainWindow::onAnimationFrameChanged(int frame) {
    // Update playback manager to apply transforms to scene
    if (playbackManager_) {
        playbackManager_->setFrame(frame);
    }

    statusBar()->showMessage(QString("Animation frame: %1").arg(frame));
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
    auto *detailsDock = new QDockWidget("Details", this);
    detailsDock->setObjectName("DetailsDock");
    detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    detailsPanel_ = new rbc::DetailsPanel(detailsDock);
    detailsDock->setWidget(detailsPanel_);
    addDockWidget(Qt::RightDockWidgetArea, detailsDock);

    // 3. Node Editor (Bottom) - use shared HttpClient
    auto *nodeDock = new QDockWidget("Node Graph", this);
    nodeDock->setObjectName("NodeDock");
    nodeDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    nodeEditor_ = new rbc::NodeEditor(httpClient_, nodeDock);
    nodeDock->setWidget(nodeEditor_);
    addDockWidget(Qt::BottomDockWidgetArea, nodeDock);

    // 4. Result Panel (Right, below Details)
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
