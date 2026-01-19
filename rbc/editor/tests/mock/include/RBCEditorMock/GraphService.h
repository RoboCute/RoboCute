#pragma once
#include <rbc_config.h>
#include "RBCEditorRuntime/services/IGraphService.h"

namespace rbc {

class RBC_EDITOR_MOCK_API MockGraphService : public IGraphService {
    Q_OBJECT

public:
    explicit MockGraphService(QObject *parent = nullptr);
    ~MockGraphService() override;

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