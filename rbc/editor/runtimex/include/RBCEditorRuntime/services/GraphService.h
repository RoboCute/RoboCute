#pragma once
#include "RBCEditorRuntime/services/IGraphService.h"

namespace rbc {

class GraphService : public IGraphService {
    Q_OBJECT
public:
    explicit GraphService(QObject *parent = nullptr);
    ~GraphService() override;

    // == IGraphService Interface ==
    // void connectToServer(const QString& serverUrl) override;
};

}// namespace rbc