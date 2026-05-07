#include "RBCEditorRuntime/services/GraphService.h"

namespace rbc {

GraphService::GraphService(QObject *parent) : IGraphService(parent) {
}

void GraphService::bindConnectionService(IConnectionService *conn_service) {
    _conn_service = conn_service;
}
void GraphService::bindProjectService(IProjectService *proj_service) {
    _proj_service = proj_service;
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
    return _is_remote_mode;
}

}// namespace rbc