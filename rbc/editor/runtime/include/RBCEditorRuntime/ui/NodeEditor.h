#pragma once
#include <rbc_config.h>
#include <QWidget>
#include <QListWidget>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>

#include "RBCEditorRuntime/infra/nodes/NodeFactory.h"
#include "RBCEditorRuntime/ui/ExecutionPanel.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API NodeEditor : public QWidget {
    Q_OBJECT
    using DataFlowGraphModel = QtNodes::DataFlowGraphModel;
    using DataFlowGraphicsScene = QtNodes::DataFlowGraphicsScene;
    using GraphicsView = QtNodes::GraphicsView;

public:
    explicit NodeEditor(QWidget *parent = nullptr);
    ~NodeEditor() override;

    void loadNodesDeferred();

private slots:
    void onConnectionStatusChanged(bool connected);
    void onHttpError(const QString &error);
    void onNodesLoaded(const QJsonArray &nodes, bool success);
    void onExecuteGraph();
    void onGraphExecuted(const QString &executionId, bool success);
    void onOutputReceived(const QJsonObject &outputs, bool success);
    void onNewGraph();
    void onSaveGraph();
    void onLoadGraph();
    void onRefreshNodes();

private:
    void setupUI();
    void loadNodesFromBackend();

private:
    std::shared_ptr<DataFlowGraphModel> m_graphModel;
    DataFlowGraphicsScene *m_scene;
    std::unique_ptr<NodeFactory> m_nodeFactory;

    // Widgets
    GraphicsView *m_view;
    QListWidget *m_nodePalette;
    ExecutionPanel *m_executionPanel;

    // State
    QString m_currentExecutionId;
    bool m_isExecuting = false;
};

}// namespace rbc