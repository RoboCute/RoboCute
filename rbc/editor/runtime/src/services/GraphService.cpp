#include "RBCEditorRuntime/services/GraphService.h"

namespace rbc {

GraphService::GraphService(QObject *parent) : IGraphService(parent) {
}
GraphService::~GraphService() {}

void GraphService::bindConnectionService(IConnectionService *conn_service) {
    m_conn_service = conn_service;
}
void GraphService::bindProjectService(IProjectService *proj_service) {
    m_proj_service = proj_service;
}

QJsonArray GraphService::getNodeDefinitions() const {
    return {};
}
QJsonObject GraphService::getNodeDefinition(const QString &nodeType) const {
    return {};
}
QMap<QString, QJsonArray> GraphService::getNodesByCategory() const {
    return {};
}

bool GraphService::isRemoteMode() const {
    return m_is_remote_mode;
}

}// namespace rbc