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
 * 执行状态枚举
 */
enum class ExecutionStatus {
    Pending,  // 等待执行
    Running,  // 正在执行
    Completed,// 执行完成
    Failed,   // 执行失败
    Cancelled // 已取消
};

/**
 * 执行进度信息
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
    virtual bool isRemoteMode() const = 0;
    virtual void bindProjectService(IProjectService *proj_service) = 0;
    virtual void bindConnectionService(IConnectionService *conn_service) = 0;

    // == Node Management
    virtual QJsonArray getNodeDefinitions() const = 0;
    virtual QJsonObject getNodeDefinition(const QString &nodeType) const = 0;
    virtual QMap<QString, QJsonArray> getNodesByCategory() const = 0;

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