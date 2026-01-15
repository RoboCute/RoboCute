#pragma once

#include <QObject>
#include <QFileSystemWatcher>
#include <QHash>
#include <QString>
#include "RBCEditorRuntime/services/IStyleManager.h"
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

class RBC_EDITOR_RUNTIME_API StyleManager : public IStyleManager {
    Q_OBJECT

public:
    explicit StyleManager(QObject *parent = nullptr);
    ~StyleManager() override = default;

    // IService interface (StyleManager implements IService through IStyleManager)
    QString serviceId() const { return "StyleManager"; }

    // QML interface
    void initialize(int argc, char **argv) override;
    bool isHotReloadEnabled() const override { return isDevMode_; }
    QUrl resolveUrl(const QString &relativePath) override;
    QQmlEngine *qmlEngine() override { return engine_; }

    // Qt Widget style interface
    bool loadGlobalStyleSheet(const QString &qssPath) override;
    bool applyStylePreset(QWidget *widget, const QString &presetName) override;
    QString getStylePreset(const QString &presetName) const override;
    void registerStylePreset(const QString &presetName, const QString &qss) override;
    void setTheme(const QString &themeName) override;
    QString currentTheme() const override { return currentTheme_; }

private:
    void loadDefaultPresets();
    void loadTheme(const QString &themeName);

    bool isDevMode_ = false;
    QString sourceRoot_;
    QQmlEngine *engine_ = nullptr;
    QFileSystemWatcher *watcher_ = nullptr;

    // Widget style management
    QHash<QString, QString> stylePresets_;// presetName -> QSS
    QString currentTheme_;
    QString globalStyleSheet_;
};

}// namespace rbc