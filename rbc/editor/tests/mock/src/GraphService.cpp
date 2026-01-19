#include "RBCEditorMock/GraphService.h"

namespace rbc {

MockGraphService::MockGraphService(QObject *parent) : IGraphService(parent) {
}
MockGraphService::~MockGraphService() {}

void MockGraphService::bindConnectionService(IConnectionService *conn_service) {
    m_conn_service = conn_service;
}
void MockGraphService::bindProjectService(IProjectService *proj_service) {
    m_proj_service = proj_service;
}

QJsonArray MockGraphService::getNodeDefinitions() const {
    return {};
}
QJsonObject MockGraphService::getNodeDefinition(const QString &nodeType) const {
    return {};
}
QMap<QString, QJsonArray> MockGraphService::getNodesByCategory() const {
    return {};
}

bool MockGraphService::isRemoteMode() const {
    return m_is_remote_mode;
}

}// namespace rbc