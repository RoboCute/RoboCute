#include "RBCEditorRuntime/WorkflowManager.h"
#include <QDebug>

namespace rbc {

WorkflowManager::WorkflowManager(QObject *parent)
    : QObject(parent),
      m_currentWorkflow(WorkflowType::SceneEditing) {
}

void WorkflowManager::switchWorkflow(WorkflowType workflow) {
    if (workflow == m_currentWorkflow) {
        return;
    }

    WorkflowType oldWorkflow = m_currentWorkflow;
    m_currentWorkflow = workflow;

    emit workflowChanged(workflow, oldWorkflow);
}

void WorkflowManager::registerWorkflow(WorkflowType type, QWidget *centralWidget,
                                       const QList<QDockWidget *> &visibleDocks,
                                       const QList<QDockWidget *> &hiddenDocks) {
    WorkflowConfig config;
    config.type = type;
    config.centralWidget = centralWidget;
    config.visibleDocks = visibleDocks;
    config.hiddenDocks = hiddenDocks;

    m_workflowConfigs[type] = config;
}

}// namespace rbc
