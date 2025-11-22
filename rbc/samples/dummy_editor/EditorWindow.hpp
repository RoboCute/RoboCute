#pragma once

#include <QMainWindow>
#include <QDockWidget>
#include <QListWidget>
#include <QToolBar>
#include <QLabel>
#include <memory>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>

#include "HttpClient.hpp"
#include "NodeFactory.hpp"
#include "ExecutionPanel.hpp"

using QtNodes::DataFlowGraphModel;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::GraphicsView;

/**
 * Main editor window
 */
class EditorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorWindow(QWidget *parent = nullptr);
    ~EditorWindow() override;

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
    void setupMenuBar();
    void setupToolBar();
    void setupDockWindows();
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

    // Dock widgets
    QDockWidget *m_nodePaletteDock;
    QListWidget *m_nodePalette;

    QDockWidget *m_executionPanelDock;
    ExecutionPanel *m_executionPanel;

    // State
    QString m_currentExecutionId;
    bool m_isExecuting;
    QString m_serverUrl;
};
