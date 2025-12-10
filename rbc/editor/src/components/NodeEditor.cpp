#include "RBCEditor/components/NodeEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QToolButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QFont>
#include <QLineEdit>
#include <QSplitter>

#include <QtNodes/ConnectionStyle>
#include "RBCEditor/components/ExecutionPanel.h"
#include "RBCEditor/nodes/DynamicNodeModel.h"

namespace rbc {

NodeEditor::NodeEditor(
    QWidget *parent) : QWidget(parent),
                       m_httpClient(new HttpClient(this)),
                       m_nodeFactory(std::make_unique<NodeFactory>()),
                       m_isExecuting(false),
                       m_serverUrl("http://127.0.0.1:5555") {
    setupUI();
    setupConnections();
    // Do not load nodes immediately - wait for proper initialization
}

NodeEditor::NodeEditor(HttpClient *httpClient, QWidget *parent) : QWidget(parent),
                                                                  m_httpClient(httpClient),
                                                                  m_nodeFactory(std::make_unique<NodeFactory>()),
                                                                  m_isExecuting(false),
                                                                  m_serverUrl(httpClient->serverUrl()) {
    setupUI();
    setupConnections();
    // Load nodes after server connection is established
    // Will be triggered by MainWindow after scene sync starts
}

NodeEditor::~NodeEditor() = default;

void NodeEditor::setupUI() {
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
    m_view = new GraphicsView(m_scene);

    // Create main layout
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Create toolbar
    m_toolBar = new QToolBar(this);
    m_toolBar->setMovable(false);

    // New graph button
    QAction *newAction = m_toolBar->addAction("New");
    connect(newAction, &QAction::triggered, this, &NodeEditor::onNewGraph);

    // Save graph button
    QAction *saveAction = m_toolBar->addAction("Save");
    connect(saveAction, &QAction::triggered, this, &NodeEditor::onSaveGraph);

    // Load graph button
    QAction *loadAction = m_toolBar->addAction("Load");
    connect(loadAction, &QAction::triggered, this, &NodeEditor::onLoadGraph);

    m_toolBar->addSeparator();

    // Execute button
    m_executeAction = m_toolBar->addAction("Execute");
    m_executeAction->setEnabled(false);
    connect(m_executeAction, &QAction::triggered, this, &NodeEditor::onExecuteGraph);

    m_toolBar->addSeparator();

    // Refresh nodes button
    QAction *refreshAction = m_toolBar->addAction("Refresh Nodes");
    connect(refreshAction, &QAction::triggered, this, &NodeEditor::onRefreshNodes);

    // Server settings button
    QAction *settingsAction = m_toolBar->addAction("Server Settings");
    connect(settingsAction, &QAction::triggered, this, &NodeEditor::onServerSettings);

    m_toolBar->addSeparator();

    // Connection status label
    m_connectionStatusLabel = new QLabel("Disconnected");
    m_connectionStatusLabel->setStyleSheet("QLabel { padding: 5px; }");
    m_toolBar->addWidget(m_connectionStatusLabel);
    updateConnectionStatus(false);

    mainLayout->addWidget(m_toolBar);

    // Create horizontal splitter for main content
    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Left side: Node palette
    m_nodePalette = new QListWidget();
    m_nodePalette->setStyleSheet("QListWidget { font-size: 11pt; }");
    m_nodePalette->setMaximumWidth(250);
    mainSplitter->addWidget(m_nodePalette);

    // Center: Graph view
    mainSplitter->addWidget(m_view);

    // Right side: Execution panel
    m_executionPanel = new ExecutionPanel(this);
    m_executionPanel->setMaximumWidth(400);
    mainSplitter->addWidget(m_executionPanel);

    // Set splitter sizes
    mainSplitter->setStretchFactor(0, 1);// Node palette
    mainSplitter->setStretchFactor(1, 3);// Graph view
    mainSplitter->setStretchFactor(2, 2);// Execution panel

    mainLayout->addWidget(mainSplitter);
}

void NodeEditor::setupConnections() {
    // HTTP client signals
    connect(m_httpClient, &HttpClient::connectionStatusChanged,
            this, &NodeEditor::onConnectionStatusChanged);
    connect(m_httpClient, &HttpClient::errorOccurred,
            this, &NodeEditor::onHttpError);

    // Scene signals
    connect(m_scene, &DataFlowGraphicsScene::sceneLoaded,
            m_view, &GraphicsView::centerScene);
    connect(m_scene, &DataFlowGraphicsScene::modified,
            this, [this]() { setWindowModified(true); });
}

void NodeEditor::loadNodesDeferred() {
    loadNodesFromBackend();
}

void NodeEditor::loadNodesFromBackend() {
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

void NodeEditor::onConnectionStatusChanged(bool connected) {
    updateConnectionStatus(connected);
}

void NodeEditor::updateConnectionStatus(bool connected) {
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

void NodeEditor::onHttpError(const QString &error) {
    m_executionPanel->logError(error);
}

void NodeEditor::onNodesLoaded(const QJsonArray &nodes, bool success) {
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
}

void NodeEditor::onExecuteGraph() {
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

    // Debug: Log graph definition
    QJsonDocument debugDoc(graphDef);
    QString debugJson = debugDoc.toJson(QJsonDocument::Indented);
    m_executionPanel->logMessage("Graph Definition:");
    m_executionPanel->logMessage(debugJson.left(500));// First 500 chars

    // Execute
    m_httpClient->executeGraph(graphDef, [this](const QString &executionId, bool success) {
        onGraphExecuted(executionId, success);
    });
}

void NodeEditor::onGraphExecuted(const QString &executionId, bool success) {
    if (!success) {
        m_executionPanel->logError("Failed to execute graph - HTTP request failed");
        m_executionPanel->logError("Check if backend server is running");
        m_executionPanel->setExecutionStatus("Execution Failed", false);
        m_isExecuting = false;
        return;
    }

    m_currentExecutionId = executionId;
    m_executionPanel->logMessage(QString("Execution ID: %1").arg(executionId));
    m_executionPanel->logMessage("Fetching execution outputs...");

    // Fetch outputs
    m_httpClient->fetchExecutionOutputs(executionId, [this](const QJsonObject &outputs, bool success) {
        onOutputsReceived(outputs, success);
    });
}

void NodeEditor::onOutputsReceived(const QJsonObject &outputs, bool success) {
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

    // Check for errors in execution
    if (status == "failed") {
        m_executionPanel->logError("Graph execution failed on server side");
        if (outputs.contains("error")) {
            m_executionPanel->logError(QString("Server error: %1").arg(outputs["error"].toString()));
        }
    }

    // Update node outputs in the graph
    if (outputs.contains("outputs")) {
        QJsonObject nodeOutputs = outputs["outputs"].toObject();

        // Iterate through all nodes in the scene and update their outputs
        for (auto it = nodeOutputs.begin(); it != nodeOutputs.end(); ++it) {
            QString nodeIdStr = it.key();
            QJsonObject outputValues = it.value().toObject();

            // Convert nodeId string to NodeId
            bool ok;
            QtNodes::NodeId nodeId = nodeIdStr.toULongLong(&ok);
            if (ok && m_graphModel->nodeExists(nodeId)) {
                // Get the node delegate model and update outputs
                auto *delegateModel = m_graphModel->delegateModel<DynamicNodeModel>(nodeId);
                if (delegateModel) {
                    delegateModel->setOutputValues(outputValues);
                }
            }

            // Find the node in the graph and update it
            highlightNodeAfterExecution(nodeIdStr, true);
        }
    }

    m_executionPanel->logMessage("=== Execution Complete ===");
}

void NodeEditor::onNewGraph() {
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

void NodeEditor::onSaveGraph() {
    if (m_scene->save()) {
        setWindowModified(false);
        m_executionPanel->logSuccess("Graph saved successfully");
    } else {
        m_executionPanel->logError("Failed to save graph");
    }
}

void NodeEditor::onLoadGraph() {
    // The scene->load() method will handle the file dialog and loading
    m_scene->load();
}

void NodeEditor::onServerSettings() {
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

void NodeEditor::onRefreshNodes() {
    m_executionPanel->logMessage("Refreshing nodes from backend...");
    loadNodesFromBackend();
}

QJsonObject NodeEditor::serializeGraph() {
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

void NodeEditor::highlightNodeAfterExecution(const QString &nodeId, bool success) {
    // This is a placeholder for node highlighting
    // In a full implementation, you would visually highlight nodes
    // based on execution status (green for success, red for error)
    Q_UNUSED(nodeId);
    Q_UNUSED(success);
}

}// namespace rbc