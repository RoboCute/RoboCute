#pragma once

#include "RBCEditorRuntime/infra/events/EventType.h"
#include <QVariant>
#include <QDateTime>

namespace rbc {

struct Event {
    EventType type;
    QVariant data;
    QObject *sender = nullptr;
    qint64 timestamp = 0;

    Event(EventType t, const QVariant &d = QVariant(), QObject *s = nullptr) : type(t), data(d), sender(s) {
        timestamp = QDateTime::currentMSecsSinceEpoch();
    }
};

}// namespace rbc