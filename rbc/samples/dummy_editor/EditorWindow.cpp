#include "EditorWindow.hpp"
#include "DynamicNodeModel.hpp"
#include <QMenuBar>
#include <QMenu>
#include <QToolButton>
#include <QStatusBar>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>

#include <QtNodes/ConnectionStyle>

EditorWindow::EditorWindow(QWidget *parent)
    : QMainWindow(parent), m_httpClient(new HttpClient(this)), m_nodeFactory(std::make_unique<NodeFactory>()), m_isExecuting(false), m_serverUrl("http://127.0.0.1:8000") {
    setupUI();
    setupConnections();

    // Set window properties
    setWindowTitle("RoboCute Node Editor");
    resize(1400, 900);

    // Load nodes from backend
    loadNodesFromBackend();
}

EditorWindow::~EditorWindow() = default;

void EditorWindow::setupUI() {
    setupMenuBar();
    setupToolBar();

    // Set up node editor style
    QtNodes::ConnectionStyle::setConnectionStyle(
        R"({
            "ConnectionStyle": {
                "ConstructionColor": "gray",
                "NormalColor": "black",
                "SelectedColor": "gray",
                "SelectedHaloColor": "deepskyblue",
                "HoveredColor": "deepskyblue",
                "LineWidth": 3.0,
                "ConstructionLineWidth": 2.0,
                "PointDiameter": 10.0,
                "UseDataDefinedColors": true
            }
        })");

    // Create graph model and scene
    m_graphModel = std::make_shared<DataFlowGraphModel>(m_nodeFactory->getRegistry());
    m_scene = new DataFlowGraphicsScene(*m_graphModel, this);

    // Create view
    m_view = new GraphicsView(m_scene);
    setCentralWidget(m_view);

    // Setup dock windows
    setupDockWindows();

    // Status bar
    statusBar()->showMessage("Ready");
}

void EditorWindow::setupMenuBar() {
    // File menu
    QMenu *fileMenu = menuBar()->addMenu("&File");

    QAction *newAction = fileMenu->addAction("&New");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &EditorWindow::onNewGraph);

    QAction *saveAction = fileMenu->addAction("&Save");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &EditorWindow::onSaveGraph);

    QAction *loadAction = fileMenu->addAction("&Open");
    loadAction->setShortcut(QKeySequence::Open);
    connect(loadAction, &QAction::triggered, this, &EditorWindow::onLoadGraph);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Execute menu
    QMenu *executeMenu = menuBar()->addMenu("&Execute");

    m_executeAction = executeMenu->addAction("&Execute Graph");
    m_executeAction->setShortcut(QKeySequence("F5"));
    connect(m_executeAction, &QAction::triggered, this, &EditorWindow::onExecuteGraph);

    // Tools menu
    QMenu *toolsMenu = menuBar()->addMenu("&Tools");

    QAction *refreshAction = toolsMenu->addAction("&Refresh Nodes");
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &EditorWindow::onRefreshNodes);

    QAction *settingsAction = toolsMenu->addAction("&Server Settings");
    connect(settingsAction, &QAction::triggered, this, &EditorWindow::onServerSettings);
}

void EditorWindow::setupToolBar() {
    m_toolBar = addToolBar("Main Toolbar");
    m_toolBar->setMovable(false);

    // Execute button
    QAction *executeAction = m_toolBar->addAction("Execute");
    connect(executeAction, &QAction::triggered, this, &EditorWindow::onExecuteGraph);

    m_toolBar->addSeparator();

    // Connection status
    m_connectionStatusLabel = new QLabel("Disconnected");
    m_connectionStatusLabel->setStyleSheet("QLabel { padding: 5px; }");
    m_toolBar->addWidget(m_connectionStatusLabel);
    updateConnectionStatus(false);
}

void EditorWindow::setupDockWindows() {
    // Node palette dock (left)
    m_nodePaletteDock = new QDockWidget("Node Palette", this);
    m_nodePalette = new QListWidget();
    m_nodePalette->setStyleSheet("QListWidget { font-size: 11pt; }");
    m_nodePaletteDock->setWidget(m_nodePalette);
    addDockWidget(Qt::LeftDockWidgetArea, m_nodePaletteDock);

    // Execution panel dock (bottom)
    m_executionPanelDock = new QDockWidget("Execution", this);
    m_executionPanel = new ExecutionPanel();
    m_executionPanelDock->setWidget(m_executionPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_executionPanelDock);
}

void EditorWindow::setupConnections() {
    // HTTP client signals
    connect(m_httpClient, &HttpClient::connectionStatusChanged,
            this, &EditorWindow::onConnectionStatusChanged);
    connect(m_httpClient, &HttpClient::errorOccurred,
            this, &EditorWindow::onHttpError);

    // Scene signals
    connect(m_scene, &DataFlowGraphicsScene::sceneLoaded,
            m_view, &GraphicsView::centerScene);
    connect(m_scene, &DataFlowGraphicsScene::modified,
            this, [this]() { setWindowModified(true); });
}

void EditorWindow::loadNodesFromBackend() {
    m_executionPanel->logMessage("Connecting to backend server...");

    // First check health
    m_httpClient->healthCheck([this](bool healthy) {
        if (healthy) {
            m_executionPanel->logSuccess("Connected to backend server");

            // Load nodes
            m_executionPanel->logMessage("Loading node types from backend...");
            m_httpClient->fetchAllNodes([this](const QJsonArray &nodes, bool success) {
                onNodesLoaded(nodes, success);
            });
        } else {
            m_executionPanel->logError("Failed to connect to backend server");
            updateConnectionStatus(false);
        }
    });
}

void EditorWindow::onConnectionStatusChanged(bool connected) {
    updateConnectionStatus(connected);
}

void EditorWindow::updateConnectionStatus(bool connected) {
    if (connected) {
        m_connectionStatusLabel->setText("Connected");
        m_connectionStatusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #90EE90; }");
        m_executeAction->setEnabled(true);
    } else {
        m_connectionStatusLabel->setText("Disconnected");
        m_connectionStatusLabel->setStyleSheet("QLabel { padding: 5px; background-color: #FFB6C1; }");
        m_executeAction->setEnabled(false);
    }
}

void EditorWindow::onHttpError(const QString &error) {
    m_executionPanel->logError(error);
    statusBar()->showMessage(error, 5000);
}

void EditorWindow::onNodesLoaded(const QJsonArray &nodes, bool success) {
    if (!success) {
        m_executionPanel->logError("Failed to load nodes from backend");
        return;
    }

    m_executionPanel->logSuccess(QString("Loaded %1 node types").arg(nodes.size()));

    // Register nodes with the factory
    m_nodeFactory->registerNodesFromMetadata(nodes, m_nodeFactory->getRegistry());

    // Update node palette
    m_nodePalette->clear();
    QMap<QString, QVector<QJsonObject>> nodesByCategory = m_nodeFactory->getNodesByCategory();

    for (auto it = nodesByCategory.begin(); it != nodesByCategory.end(); ++it) {
        QString category = it.key();

        // Add category header
        auto headerItem = new QListWidgetItem(QString("=== %1 ===").arg(category));
        headerItem->setFlags(Qt::ItemIsEnabled);
        QFont font = headerItem->font();
        font.setBold(true);
        headerItem->setFont(font);
        m_nodePalette->addItem(headerItem);

        // Add nodes in category
        for (const QJsonObject &nodeMetadata : it.value()) {
            QString displayName = nodeMetadata["display_name"].toString();
            QString nodeType = nodeMetadata["node_type"].toString();
            QString description = nodeMetadata["description"].toString();

            auto item = new QListWidgetItem(QString("  %1").arg(displayName));
            item->setToolTip(QString("%1\n\nType: %2\n%3")
                                 .arg(displayName, nodeType, description));
            item->setData(Qt::UserRole, nodeType);
            m_nodePalette->addItem(item);
        }
    }

    statusBar()->showMessage(QString("Loaded %1 node types").arg(nodes.size()), 3000);
}

void EditorWindow::onExecuteGraph() {
    if (m_isExecuting) {
        m_executionPanel->logError("Already executing a graph");
        return;
    }

    m_executionPanel->logMessage("=== Starting Graph Execution ===");
    m_executionPanel->setExecutionStatus("Executing...", true);
    m_isExecuting = true;

    // Serialize graph
    QJsonObject graphDef = serializeGraph();

    // Log graph info
    int nodeCount = graphDef["nodes"].toArray().size();
    int connectionCount = graphDef["connections"].toArray().size();
    m_executionPanel->logMessage(QString("Graph: %1 nodes, %2 connections")
                                     .arg(nodeCount)
                                     .arg(connectionCount));

    // Execute
    m_httpClient->executeGraph(graphDef, [this](const QString &executionId, bool success) {
        onGraphExecuted(executionId, success);
    });
}

void EditorWindow::onGraphExecuted(const QString &executionId, bool success) {
    if (!success) {
        m_executionPanel->logError("Failed to execute graph");
        m_executionPanel->setExecutionStatus("Execution Failed", false);
        m_isExecuting = false;
        return;
    }

    m_currentExecutionId = executionId;
    m_executionPanel->logMessage(QString("Execution ID: %1").arg(executionId));

    // Fetch outputs
    m_httpClient->fetchExecutionOutputs(executionId, [this](const QJsonObject &outputs, bool success) {
        onOutputsReceived(outputs, success);
    });
}

void EditorWindow::onOutputsReceived(const QJsonObject &outputs, bool success) {
    m_isExecuting = false;

    if (!success) {
        m_executionPanel->logError("Failed to retrieve execution outputs");
        m_executionPanel->setExecutionStatus("Execution Failed", false);
        return;
    }

    QString status = outputs["status"].toString();
    m_executionPanel->logSuccess(QString("Execution completed: %1").arg(status));
    m_executionPanel->setExecutionStatus("Execution Completed", false);

    // Display results
    m_executionPanel->displayExecutionResults(outputs);

    // Update node outputs in the graph
    if (outputs.contains("outputs")) {
        QJsonObject nodeOutputs = outputs["outputs"].toObject();

        // Iterate through all nodes in the scene and update their outputs
        for (auto it = nodeOutputs.begin(); it != nodeOutputs.end(); ++it) {
            QString nodeId = it.key();
            QJsonObject outputValues = it.value().toObject();

            // Find the node in the graph and update it
            // Note: This is simplified - in a real implementation, you'd need to
            // map node IDs to actual node instances in the scene
            highlightNodeAfterExecution(nodeId, true);
        }
    }

    m_executionPanel->logMessage("=== Execution Complete ===");
}

QJsonObject EditorWindow::serializeGraph()
{
    QJsonObject graphDef;
    QJsonArray nodesArray;
    QJsonArray connectionsArray;
    
    // Get all nodes from the graph model
    auto nodeIds = m_graphModel->allNodeIds();
    
    for (const auto &nodeId : nodeIds) {
        // Get the node delegate model
        auto *delegateModel = m_graphModel->delegateModel<DynamicNodeModel>(nodeId);
        if (!delegateModel) continue;
        
        QJsonObject nodeObj;
        nodeObj["node_id"] = QString::number(nodeId);
        nodeObj["node_type"] = delegateModel->nodeType();
        nodeObj["inputs"] = delegateModel->getInputValues();
        
        nodesArray.append(nodeObj);
    }
    
    // Get all connections by iterating through all nodes
    std::unordered_set<QtNodes::ConnectionId> allConnections;
    for (const auto &nodeId : nodeIds) {
        auto nodeConnections = m_graphModel->allConnectionIds(nodeId);
        allConnections.insert(nodeConnections.begin(), nodeConnections.end());
    }
    
    for (const auto &connId : allConnections) {
        QJsonObject connObj;
        connObj["from_node"] = QString::number(connId.outNodeId);
        connObj["to_node"] = QString::number(connId.inNodeId);
        
        // Get port names from the delegate models
        auto *outDelegate = m_graphModel->delegateModel<DynamicNodeModel>(connId.outNodeId);
        auto *inDelegate = m_graphModel->delegateModel<DynamicNodeModel>(connId.inNodeId);
        
        if (outDelegate && inDelegate) {
            connObj["from_output"] = outDelegate->portCaption(QtNodes::PortType::Out, connId.outPortIndex);
            connObj["to_input"] = inDelegate->portCaption(QtNodes::PortType::In, connId.inPortIndex);
        }
        
        connectionsArray.append(connObj);
    }
    
    graphDef["nodes"] = nodesArray;
    graphDef["connections"] = connectionsArray;
    graphDef["metadata"] = QJsonObject();
    
    return graphDef;
}

void EditorWindow::highlightNodeAfterExecution(const QString &nodeId, bool success) {
    // This is a placeholder for node highlighting
    // In a full implementation, you would visually highlight nodes
    // based on execution status (green for success, red for error)
    Q_UNUSED(nodeId);
    Q_UNUSED(success);
}

void EditorWindow::onNewGraph()
{
    if (isWindowModified()) {
        auto reply = QMessageBox::question(this, "New Graph",
            "Current graph has unsaved changes. Continue?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply != QMessageBox::Yes) {
            return;
        }
    }
    
    // Clear all nodes from the graph
    auto nodeIds = m_graphModel->allNodeIds();
    for (const auto &nodeId : nodeIds) {
        m_graphModel->deleteNode(nodeId);
    }
    
    m_executionPanel->clearConsole();
    m_executionPanel->clearResults();
    setWindowModified(false);
    m_executionPanel->logMessage("Created new graph");
}

void EditorWindow::onSaveGraph() {
    if (m_scene->save()) {
        setWindowModified(false);
        m_executionPanel->logSuccess("Graph saved successfully");
        statusBar()->showMessage("Graph saved", 3000);
    } else {
        m_executionPanel->logError("Failed to save graph");
    }
}

void EditorWindow::onLoadGraph() {
    // The scene->load() method will handle the file dialog and loading
    m_scene->load();
}

void EditorWindow::onServerSettings() {
    bool ok;
    QString newUrl = QInputDialog::getText(this, "Server Settings",
                                           "Backend Server URL:", QLineEdit::Normal, m_serverUrl, &ok);

    if (ok && !newUrl.isEmpty()) {
        m_serverUrl = newUrl;
        m_httpClient->setServerUrl(newUrl);
        m_executionPanel->logMessage(QString("Server URL changed to: %1").arg(newUrl));

        // Reconnect and reload nodes
        loadNodesFromBackend();
    }
}

void EditorWindow::onRefreshNodes() {
    m_executionPanel->logMessage("Refreshing nodes from backend...");
    loadNodesFromBackend();
}
