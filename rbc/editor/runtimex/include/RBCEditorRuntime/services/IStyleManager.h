#pragma once

#include <rbc_config.h>
#include <QObject>
#include <QQmlEngine>
#include <QWidget>
#include <QString>

namespace rbc {

class RBC_EDITOR_RUNTIME_API IStyleManager : public QObject {
    Q_OBJECT
public:
    explicit IStyleManager(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~IStyleManager() = default;

    // QML interface
    virtual void initialize(int argc, char **argv) = 0;
    virtual bool isHotReloadEnabled() const { return false; }
    virtual QUrl resolveUrl(const QString &relativePath) = 0;
    virtual QQmlEngine *qmlEngine() = 0;

    // Qt Widget style interface
    /**
     * @brief Load and apply global stylesheet
     * @param qssPath Path to QSS file (can be resource path like ":/main.qss")
     * @return true if loaded successfully
     */
    virtual bool loadGlobalStyleSheet(const QString &qssPath) = 0;

    /**
     * @brief Apply style preset to a widget
     * @param widget Target widget
     * @param presetName Name of the preset (e.g., "FileTree", "StatusLabel")
     * @return true if preset exists and applied successfully
     */
    virtual bool applyStylePreset(QWidget *widget, const QString &presetName) = 0;

    /**
     * @brief Get style preset QSS string
     * @param presetName Name of the preset
     * @return QSS string, empty if preset not found
     */
    virtual QString getStylePreset(const QString &presetName) const = 0;

    /**
     * @brief Register a custom style preset
     * @param presetName Name of the preset
     * @param qss QSS string for the preset
     */
    virtual void registerStylePreset(const QString &presetName, const QString &qss) = 0;

    /**
     * @brief Set current theme
     * @param themeName Theme name (e.g., "dark", "light")
     */
    virtual void setTheme(const QString &themeName) = 0;

    /**
     * @brief Get current theme name
     */
    virtual QString currentTheme() const = 0;

signals:
    /**
     * @brief Emitted when theme changes
     */
    void themeChanged(const QString &themeName);
};

}// namespace rbc