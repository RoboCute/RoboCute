#pragma once

#include <QObject>
#include <QQmlEngine>

namespace rbc {

class IStyleManager : public QObject {
    Q_OBJECT
public:
    // interface
    virtual void initialize(int argc, char **argv) = 0;
    virtual bool isHotReloadEnabled() const { return false; }
    virtual QUrl resolveUrl(const QString &relativePath) = 0;
    virtual QQmlEngine *qmlEngine() = 0;
};

}// namespace rbc