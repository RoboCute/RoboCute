#pragma once

#include <QObject>
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

class ISceneService : public IService {
    Q_OBJECT
public:
    virtual ~ISceneService() = default;

    // Scene Operations
    // virtual Entity *getEntity(int entityId) const = 0;
    QString serviceId() const override { return "com.robocute.scene_service"; }

signals:
};

}// namespace rbc