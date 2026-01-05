#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include "RBCEditorRuntime/services/IStyleManager.h"

namespace rbc {

class StyleManager : public IStyleManager {
    Q_OBJECT

public:
    explicit StyleManager(QObject *parent = nullptr);
    void initialize(int argc, char **argv) override;
    bool isHotReloadEnabled() const override { return isDevMode_; }
    QUrl resolveUrl(const QString &relativePath) override;
    QQmlEngine *qmlEngine() override { return engine_; }

private:
    bool isDevMode_ = false;
    QString sourceRoot_;
    QQmlEngine *engine_ = nullptr;
    QFileSystemWatcher *watcher_ = nullptr;
};

}// namespace rbc