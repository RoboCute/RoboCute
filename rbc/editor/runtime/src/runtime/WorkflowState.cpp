#include "RBCEditorRuntime/runtime/WorkflowState.h"
#include <QDebug>

namespace rbc {

WorkflowState::WorkflowState(QObject* parent)
    : QObject(parent) {
}

WorkflowState::~WorkflowState() {
    disconnectAll();
}

void WorkflowState::addConnection(const QMetaObject::Connection& connection) {
    if (connection) {
        connections_.append(connection);
    }
}

void WorkflowState::disconnectAll() {
    for (const auto& conn : connections_) {
        QObject::disconnect(conn);
    }
    connections_.clear();
}

}// namespace rbc

