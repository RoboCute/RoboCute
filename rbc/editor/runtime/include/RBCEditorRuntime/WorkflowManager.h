#pragma once

#include <QObject>
#include <QWidget>
#include <QDockWidget>
#include <QList>
#include <QMap>

namespace rbc {

enum class WorkflowType {
    SceneEditing,  // Default workflow with Viewport as central widget
    Text2Image     // Text2Image workflow with NodeGraph as central widget
};

/**
 * Configuration for a workflow
 */
struct WorkflowConfig {
    WorkflowType type;
    QWidget* centralWidget;
    QList<QDockWidget*> visibleDocks;
    QList<QDockWidget*> hiddenDocks;
};

/**
 * Manages workflow switching and layout configuration
 */
class WorkflowManager : public QObject {
    Q_OBJECT

public:
    explicit WorkflowManager(QObject *parent = nullptr);
    ~WorkflowManager() override = default;

    // Get current workflow
    WorkflowType currentWorkflow() const { return m_currentWorkflow; }

    // Switch workflow
    void switchWorkflow(WorkflowType workflow);

    // Register widgets and docks for a workflow
    void registerWorkflow(WorkflowType type, QWidget* centralWidget,
                         const QList<QDockWidget*>& visibleDocks,
                         const QList<QDockWidget*>& hiddenDocks = {});

signals:
    void workflowChanged(WorkflowType newWorkflow, WorkflowType oldWorkflow);

private:
    WorkflowType m_currentWorkflow;
    QMap<WorkflowType, WorkflowConfig> m_workflowConfigs;
};

}// namespace rbc

