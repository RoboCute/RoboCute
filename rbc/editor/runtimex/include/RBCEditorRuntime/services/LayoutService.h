#pragma once
/**
 * Default LayoutService for RBC Editor
 */
#include <rbc_config.h>
#include <QMainWindow>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QSet>
#include <QVBoxLayout>
#include "RBCEditorRuntime/services/ILayoutService.h"
#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"

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
    QWidget *centralWidget = nullptr;// for center widgets not in dock
    QObject *viewModel = nullptr;
    bool isCentralWidget = false;// true if this view is the central widget
};

struct LayoutConfig {
    QString layoutId;
    QString layoutName;
    QString description;
    QString version;
    QMap<QString, ViewState> views;// viewId -> ViewState
    QJsonObject dockArrangements;
    QString centralWidgetId;// viewId of the central widget
    bool isBuiltIn = false;
    bool isModified = false;
};

class RBC_EDITOR_RUNTIME_API LayoutService : public ILayoutService {
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
    // extra public
    
    /**
     * @brief Clear the current layout - remove all dock widgets and reset central widget
     * 
     * This must be called before applying a new layout to avoid duplicate dock widgets.
     * Views that will be reused in the new layout are preserved (hidden, not deleted).
     * 
     * @param preserveViews Set of view IDs that should be preserved for reuse
     */
    void clearCurrentLayout(const QSet<QString> &preserveViews = {});
    
    void loadBuiltInLayouts();// from source file
    void loadUserLayouts();   // load from user-setting file
    QString layoutConfigDirectory() const;

    /**
     * @brief Set central widget for the current layout
     * 
     * Finds the plugin that owns the specified viewId and sets its widget as central widget.
     * 
     * @param viewId The view ID to set as central widget
     * @return true if successful, false otherwise
     */
    bool setCentralWidget(const QString &viewId);

    /**
     * @brief Create dock widget for a view
     * 
     * If the view's plugin provides a widget, creates a dock with that widget.
     * Otherwise, creates a placeholder dock.
     * 
     * @param viewState The view state from layout config
     * @return The created QDockWidget, or nullptr if failed
     */
    QDockWidget *createViewDock(const ViewState &viewState);

private:// helpers
    /**
     * @brief Create a placeholder dock widget for undefined view
     * 
     * @param viewId The view ID
     * @param title The title to display
     * @param dockArea The dock area
     * @return The created placeholder QDockWidget
     */
    QDockWidget *createPlaceholderDock(const QString &viewId, const QString &title, const QString &dockArea);
    /**
     * @brief Parse layout config from JSON object
     */
    bool parseLayoutConfig(const QJsonObject &json, LayoutConfig &config);

    /**
     * @brief Convert dock area string to Qt::DockWidgetArea
     */
    static Qt::DockWidgetArea parseDockArea(const QString &dockArea);

    /**
     * @brief Find plugin that provides the specified view
     */
    IEditorPlugin *findPluginForView(const QString &viewId);

    /**
     * @brief Apply dock arrangements (splits, tabs, etc.)
     */
    void applyDockArrangements(const QJsonObject &arrangements);

private:// members
    WindowManager *windowManager_ = nullptr;
    EditorPluginManager *pluginManager_ = nullptr;
    QString currentLayoutId_;
    QMap<QString, LayoutConfig> layouts_;
    QMap<QString, ViewState> currentViewStates_;

    bool isTransitioning_ = false;
    QTimer *transitionTimer_ = nullptr;
    QString configDirectory_;
    
    // Central widget container - a stable wrapper for the actual central widget
    // This prevents Qt from deleting the actual widget when switching layouts
    QWidget *centralWidgetContainer_ = nullptr;
    QVBoxLayout *centralContainerLayout_ = nullptr;
    QString currentCentralViewId_;  // viewId of the current central widget
};

}// namespace rbc