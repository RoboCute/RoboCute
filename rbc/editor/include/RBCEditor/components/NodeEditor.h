#pragma once
#include <QWidget>
#include <QListWidget>
#include <QToolBar>
#include <QLabel>
#include <QSplitter>
#include <memory>
#include <unordered_set>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>

#include "RBCEditor/runtime/HttpClient.h"
#include "RBCEditor/nodes/NodeFactory.h"
#include "RBCEditor/components/ExecutionPanel.h"

namespace rbc {
using QtNodes::DataFlowGraphModel;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::GraphicsView;

class NodeEditor : public QWidget {
    Q_OBJECT
public:
    explicit NodeEditor(QWidget *parent);
    explicit NodeEditor(HttpClient *httpClient, QWidget *parent);
    ~NodeEditor() override;
    
    // Load nodes from backend (called after server is connected)
    void loadNodesDeferred();

    // Set as central widget mode (adjusts layout for central widget usage)
    void setAsCentralWidget(bool isCentral);

private slots:
    void onConnectionStatusChanged(bool connected);
    void onHttpError(const QString &error);
    void onNodesLoaded(const QJsonArray &nodes, bool success);
    void onExecuteGraph();
    void onGraphExecuted(const QString &executionId, bool success);
    void onOutputsReceived(const QJsonObject &outputs, bool success);
    void onNewGraph();
    void onSaveGraph();
    void onLoadGraph();
    void onServerSettings();
    void onRefreshNodes();


private:
    void setupUI();
    // void setupMenuBar();
    // void setupToolBar();
    // void setupDockWindows();
    void setupConnections();
    void loadNodesFromBackend();
    void updateConnectionStatus(bool connected);

    QJsonObject serializeGraph();
    void highlightNodeAfterExecution(const QString &nodeId, bool success);

    // HTTP client
    HttpClient *m_httpClient;

    // Node factory
    std::unique_ptr<NodeFactory> m_nodeFactory;

    // Node editor components
    std::shared_ptr<DataFlowGraphModel> m_graphModel;
    DataFlowGraphicsScene *m_scene;
    GraphicsView *m_view;

    // UI components
    QToolBar *m_toolBar;
    QLabel *m_connectionStatusLabel;
    QAction *m_executeAction;

    // Widgets
    QListWidget *m_nodePalette;
    ExecutionPanel *m_executionPanel;
    QSplitter *m_mainSplitter;

    // State
    QString m_currentExecutionId;
    bool m_isExecuting;
    QString m_serverUrl;
    bool m_isCentralWidget;
};

}// namespace rbc