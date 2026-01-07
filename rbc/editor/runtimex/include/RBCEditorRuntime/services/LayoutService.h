#pragma once
/**
 * Default LayoutService for RBC Editor
 */
#include <QMainWindow>
#include "RBCEditorRuntime/services/ILayoutService.h"

namespace rbc {

struct ViewState {
    QString pluginId;
    QString viewId;
    QString dockArea;
    bool visible = true;
    bool minimized = false;
    int width = -1;// -1 means auto
    int height = -1;
    QString tabGroup;
    int tabIndex = 0;
    QJsonObject properties;
    QDockWidget *dockWidget = nullptr;
    QObject *viewModel = nullptr;
};

struct LayoutConfig {
    QString layoutId;
    QString layoutName;
    QString description;
    QString version;
    QMap<QString, ViewState> views;// viewId -> ViewState
    QJsonObject dockArrangements;
    bool isBuiltIn = false;
    bool isModified = false;
};

class LayoutService : public ILayoutService {
    Q_OBJECT
public:
    explicit LayoutService(QObject *parent = nullptr);
    virtual ~LayoutService();

    // ILayoutService Impl
    QString currentLayoutId() const override;
    QStringList availableLayouts() const override;
    QJsonObject getLayoutMetadata(const QString &layoutId) const override;
    bool hasLayout(const QString &layoutId) const override;

    bool switchToLayout(const QString &layoutId, bool saveCurrentLayout = true) override;
    bool saveCurrentLayout(const QString &layoutId = QString()) override;
    void resetLayout(const QString &layoutId) override;

    bool createLayout(const QString &layoutId, const QJsonObject &layoutConfig) override;
    bool loadLayoutFromFile(const QString &filePath) override;
    bool saveLayoutToFile(const QString &layoutId, const QString &filePath) override;
    bool deleteLayout(const QString &layoutId) override;
    bool cloneLayout(const QString &sourceId, const QString &newId, const QString &newName) override;

    void setViewVisible(const QString &viewId, bool visible) override;
    bool isViewVisible(const QString &viewId) const override;
    void resizeView(const QString &viewId, int width, int height) override;
    void moveViewToDockArea(const QString &viewId, const QString &dockArea) override;

    void initialize(WindowManager *windowManager, EditorPluginManager *pluginManager) override;
    void applyLayout(const QString &layoutId) override;

public:
    // extra plubic
    void loadBuiltInLayouts();// from source file
    void loadUserLayouts();   // load from user-setting file
    QString layoutConfigDirectory() const;

private:// helpers

private:// members
    WindowManager *windowManager_ = nullptr;
    EditorPluginManager *pluginManager_ = nullptr;
    QString currentLayoutId_;
    QMap<QString, LayoutConfig> layouts_;
    QMap<QString, ViewState> currentViewStates_;

    bool isTransitioning_ = false;
    QTimer *transitionTimer_ = nullptr;
    QString configDirectory_;
};

}// namespace rbc