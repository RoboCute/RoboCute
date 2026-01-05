#pragma once

#include <QObject>

namespace rbc {

class ISceneService : public QObject {
    Q_OBJECT
public:
    virtual ~ISceneService() = default;

    // Scene Operations
    // virtual Entity *getEntity(int entityId) const = 0;

signals:
};

}// namespace rbc