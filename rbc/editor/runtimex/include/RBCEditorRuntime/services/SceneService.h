#pragma once

#include <rbc_config.h>
#include <QObject>
#include "RBCEditorRuntime/services/ISceneService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API SceneService : public ISceneService {
    Q_OBJECT
public:
    explicit SceneService(QObject *parent = nullptr);
    virtual ~SceneService();
};

}// namespace rbc