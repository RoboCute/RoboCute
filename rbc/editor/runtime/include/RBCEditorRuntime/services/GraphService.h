#pragma once
#include "RBCEditorRuntime/services/IGraphService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API GraphService : public IGraphService {
    Q_OBJECT
public:
    explicit GraphService(QObject *parent = nullptr);
    ~GraphService() override;

    // == IGraphService Interface ==
    bool isRemoteMode() const override;
    void bindProjectService(IProjectService *proj_service) override;
    void bindConnectionService(IConnectionService *conn_service) override;
    QJsonArray getNodeDefinitions() const override;
    QJsonObject getNodeDefinition(const QString &nodeType) const override;
    QMap<QString, QJsonArray> getNodesByCategory() const override;


private:
    IProjectService *m_proj_service = nullptr;
    IConnectionService *m_conn_service = nullptr;
    bool m_is_remote_mode = false;
};

}// namespace rbc