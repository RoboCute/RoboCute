#pragma once
#include "RBCEditorRuntime/services/IGraphService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API GraphService : public IGraphService {
    Q_OBJECT
public:
    explicit GraphService(QObject *parent = nullptr);
    ~GraphService() override = default;

    // == IGraphService Interface ==
    bool isRemoteMode() const override;
    void bindProjectService(IProjectService *proj_service) override;
    void bindConnectionService(IConnectionService *conn_service) override;
    QJsonArray getNodeDefinitions() const override;
    QJsonObject getNodeDefinition(const QString &nodeType) const override;
    QMap<QString, QJsonArray> getNodesByCategory() const override;


private:
    IProjectService *_proj_service = nullptr;
    IConnectionService *_conn_service = nullptr;
    bool _is_remote_mode = false;
};

}// namespace rbc