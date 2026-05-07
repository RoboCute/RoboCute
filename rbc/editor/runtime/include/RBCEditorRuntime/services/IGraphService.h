#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include "RBCEditorRuntime/services/IProjectService.h"
#include "RBCEditorRuntime/services/IConnectionService.h"

namespace rbc {

/**
 * Execution status enum
 */
enum class ExecutionStatus {
    Pending,  // Pending execution
    Running,  // Currently running
    Completed,// Execution completed
    Failed,   // Execution failed
    Cancelled // Execution cancelled
};

/**
 * Execution progress info
 */
struct ExecutionProgress {
    QString executionId;
    ExecutionStatus status;
    int totalNodes;
    int completedNodes;
    QString currentNodeId;
    QString currentNodeName;
    double progressPercent;
    QString message;
};

class RBC_EDITOR_RUNTIME_API IGraphService : public QObject {
    Q_OBJECT
public:
    explicit IGraphService(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IGraphService() = default;

    // === Service Dependencies ===
    [[nodiscard]] virtual bool isRemoteMode() const = 0;
    virtual void bindProjectService(IProjectService *proj_service) = 0;
    virtual void bindConnectionService(IConnectionService *conn_service) = 0;

    // == Node Management
    [[nodiscard]] virtual QJsonArray getNodeDefinitions() const = 0;
    [[nodiscard]] virtual QJsonObject getNodeDefinition(const QString &nodeType) const = 0;
    [[nodiscard]] virtual QMap<QString, QJsonArray> getNodesByCategory() const = 0;

    // // == Graph Management
    // virtual QString currentGraphId() const = 0;
    // virtual bool switchToGraph(const QString &graphId) = 0;
    // virtual bool closeGraph(const QString &graphId) = 0;
    // virtual bool loadGraphDefinition(const QString &graphId, const QJsonObject &definition) = 0;

    // // == Execution Management
    // virtual bool IsCurrentGraphExecutable() = 0;// check executable
    // virtual bool IsGraphExecutable(const QString &graphId);
    // virtual QString executeCurrentGraph() = 0;
    // virtual QString executeGraph(const QString &graphId) = 0;
    // virtual void cancelExecution(const QString &executionId) = 0;
    // virtual ExecutionStatus getExecutionStatus(const QString &executionId) const = 0;
    // virtual QStringList getActiveExecutions() const = 0;

signals:
    // void connectionStatusChanged(bool connected);
    // void connectionError(const QString &error);
    // void nodeDefinitionsUpdated();
    // void nodeDefinitionsLoadFailed(const QString &error);
    // void graphCreated(const QString &graphId);
    // void graphClosed(const QString &graphId);
    // void currentGraphChanged(const QString &graphId);
    // void graphModified(const QString &graphId);

    // void executionStarted(const QString &executionId, const QString &graphId);
    // void executionProgress(const ExecutionProgress &progress);
    // void executionCompleted(const QString &executionId);
    // void executionFailed(const QString &executionId, const QString &error);
    // void executionCancelled(const QString &executionId);

    // void nodeExecutionStarted(const QString &executionId, const QString &nodeId);
    // void nodeExecutionCompleted(const QString &executionId, const QString &nodeId);
    // void nodeExecutionFailed(const QString &executionId, const QString &nodeId, const QString &error);
};

}// namespace rbc