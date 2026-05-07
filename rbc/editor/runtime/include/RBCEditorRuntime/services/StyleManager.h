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
    [[nodiscard]] QString serviceId() const { return "StyleManager"; }

    // QML interface
    void initialize(int argc, char **argv) override;
    bool isHotReloadEnabled() const override { return _is_dev_mode; }
    QUrl resolveUrl(const QString &relativePath) override;
    QQmlEngine *qmlEngine() override { return _engine; }

    // Qt Widget style interface
    bool loadGlobalStyleSheet(const QString &qssPath) override;
    bool applyStylePreset(QWidget *widget, const QString &presetName) override;
    QString getStylePreset(const QString &presetName) const override;
    void registerStylePreset(const QString &presetName, const QString &qss) override;
    void setTheme(const QString &themeName) override;
    QString currentTheme() const override { return _current_theme; }

private:
    void loadDefaultPresets();
    void loadTheme(const QString &themeName);

    bool _is_dev_mode = false;
    QString _source_root;
    QQmlEngine *_engine = nullptr;
    QFileSystemWatcher *_watcher = nullptr;

    // Widget style management
    QHash<QString, QString> _style_presets;// presetName -> QSS
    QString _current_theme;
    QString _global_style_sheet;
};

}// namespace rbc