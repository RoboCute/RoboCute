#include "RBCEditorRuntime/ui/NodeEditor.h"
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
#include "RBCEditorRuntime/ui/ExecutionPanel.h"
#include "RBCEditorRuntime/infra/nodes/DynamicNodeModel.h"

namespace rbc {

NodeEditor::NodeEditor(QWidget *parent)
    : QWidget(parent),
      _node_factory(std::make_unique<NodeFactory>()) {
    setupUI();
}
NodeEditor::~NodeEditor() = default;

void NodeEditor::setupUI() {
    // setup node editor style
    QtNodes::ConnectionStyle::setConnectionStyle(R"({
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

    _graph_model = std::make_shared<DataFlowGraphModel>(_node_factory->getRegistry());
    _scene = new DataFlowGraphicsScene(*_graph_model, this);
    _view = new GraphicsView(_scene);
    _view->setAcceptDrops(true);

    // create Main Layout
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // create toolbar
    // m_toolBar = new QToolBar(this);
    // mainLayout->addWidget(m_toolBar)
    auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
    // Left: Node Pallete
    _node_palette = new QListWidget();
    _node_palette->setStyleSheet("QListWidget { font-size: 11pt; }");
    _node_palette->setMaximumWidth(250);
    mainSplitter->addWidget(_node_palette);

    // Center: Graph View
    mainSplitter->addWidget(_view);

    // Right: Execution Panel
    _execution_panel = new ExecutionPanel(this);
    _execution_panel->setMaximumWidth(400);
    mainSplitter->addWidget(_execution_panel);

    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);// Graph view
    mainSplitter->setStretchFactor(2, 2);// Execution panel

    mainLayout->addWidget(mainSplitter);
}

void NodeEditor::onConnectionStatusChanged(bool connected) {}
void NodeEditor::onHttpError(const QString &error) {}
void NodeEditor::onNodesLoaded(const QJsonArray &nodes, bool success) {
    if (!success) {
        _execution_panel->logError("Failed to load nodes from backend");
        return;
    }
    _execution_panel->logSuccess(QString("Loaded %1 node types").arg(nodes.size()));
    // Register nodes with the factory
    _node_factory->registerNodesFromMetadata(nodes, _node_factory->getRegistry());

    _node_palette->clear();
    QMap<QString, QVector<QJsonObject>> nodesByCategory = _node_factory->getNodesByCategory();

    for (auto it = nodesByCategory.begin(); it != nodesByCategory.end(); ++it) {
        QString category = it.key();

        // Add category header
        auto headerItem = new QListWidgetItem(QString("=== %1 ===").arg(category));
        headerItem->setFlags(Qt::ItemIsEnabled);
        QFont font = headerItem->font();
        font.setBold(true);
        headerItem->setFont(font);
        _node_palette->addItem(headerItem);

        // Add nodes in category
        for (const QJsonObject &nodeMetadata : it.value()) {
            QString displayName = nodeMetadata["display_name"].toString();
            QString nodeType = nodeMetadata["node_type"].toString();
            QString description = nodeMetadata["description"].toString();

            auto item = new QListWidgetItem(QString("  %1").arg(displayName));
            item->setToolTip(QString("%1\n\nType: %2\n%3")
                                 .arg(displayName, nodeType, description));
            item->setData(Qt::UserRole, nodeType);
            _node_palette->addItem(item);
        }
    }
}
void NodeEditor::onExecuteGraph() {}
void NodeEditor::onGraphExecuted(const QString &executionId, bool success) {}
void NodeEditor::onOutputReceived(const QJsonObject &outputs, bool success) {}
void NodeEditor::onNewGraph() {}
void NodeEditor::onSaveGraph() {}
void NodeEditor::onLoadGraph() {}
void NodeEditor::onRefreshNodes() {}

void NodeEditor::loadNodesDeferred() {
    loadNodesFromBackend();
}

void NodeEditor::loadNodesFromBackend() {
    _execution_panel->logMessage("Connecting to backend server...");
    QJsonObject input = {
        {"name", "value"},
        {"type", "number"},
        {"required", true},
        {"default", 0.0},
        {"desciprtion", "value"}};
    QJsonArray inputs = {
        input};

    QJsonObject output = {
        {"name", "output"},
        {"type", "number"},
        {"description", "output value"}};
    QJsonArray outputs = {
        output};

    QJsonObject node_def = {{"node_type", "input_number"}, {"display_name", "Input Number"}, {"category", "input"}, {"description", "Provide a numeric input"}, {"inputs", inputs}, {"outputs", outputs}};

    QJsonArray nodes = {
        node_def};
    onNodesLoaded(nodes, true);

    // m_httpClient->healthCheck([this](bool healthy) {
    //     if (healthy) {
    //         _execution_panel->logSuccess("Connected to backend server");

    //         // Load nodes
    //         _execution_panel->logMessage("Loading node types from backend...");
    //         m_httpClient->fetchAllNodes([this](const QJsonArray &nodes, bool success) {
    //             onNodesLoaded(nodes, success);
    //         });
    //     } else {
    //         _execution_panel->logError("Failed to connect to backend server");
    //         updateConnectionStatus(false);
    //     }
    // });
}

}// namespace rbc