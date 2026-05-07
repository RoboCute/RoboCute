#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QPointer>
#include <QMainWindow>
#include <QDockWidget>
#include <QWidget>
#include <QHash>
#include "RBCEditorRuntime/plugins/PluginContributions.h"

namespace rbc {

class EditorPluginManager;

/**
 * @brief WindowManager - Qt Widget lifecycle management paradigm
 *
 * Design principles:
 * 1. Follow Qt parent-child ownership model: child widgets are automatically managed by parent widget
 * 2. WindowManager owns _main_window, which owns all its child widgets
 * 3. Externally passed widgets (e.g., created by plugins) are tracked using QPointer, ownership not taken
 * 4. cleanup() must be called before plugin unload to ensure correct destruction order
 * 5. Destructor is kept simple, relying on Qt's automatic cleanup mechanism
 */
class RBC_EDITOR_RUNTIME_API WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(EditorPluginManager *plugin_mng, QObject *parent = nullptr);
    ~WindowManager() override;

    // == Lifecycle Management ==
    /**
     * @brief Call this method before plugin unload for cleanup
     *
     * This is a critical lifecycle method:
     * - Hide window and stop rendering
     * - Clean up QML context references (break references to ViewModel)
     * - Release external widget references (let plugin manage its widgets)
     * - Do not delete any widgets, let the destructor handle via Qt mechanism
     */
    void cleanup();

    // == MainWindow Management ==
    QMainWindow *main_window() const { return _main_window; }
    void setup_main_window();

    // == Create Dockable Window View through Contribution (QML)
    QDockWidget *createDockableView(const ViewContribution &contribution, QObject *viewModel);

    // == Create Dockable Window View through native QWidget (C++ widget)
    // Note: widget ownership is transferred to DockWidget (i.e., _main_window)
    // If widget comes from external source (e.g., plugin), caller must ensure widget is not destroyed before WindowManager
    // Use isExternalWidget=true to indicate widget is from external source, WindowManager will not delete it during destruction
    QDockWidget *createDockableView(
        const QString &viewId,
        const QString &title,
        QWidget *widget,
        Qt::DockWidgetArea dockArea,
        QDockWidget::DockWidgetFeatures features = (QDockWidget::DockWidgetClosable |
                                                   QDockWidget::DockWidgetMovable |
                                                   QDockWidget::DockWidgetFloatable),
        Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
        bool isExternalWidget = false);

    // == Create Dockable View from NativeViewContribution
    // Convenience method: create DockWidget directly from NativeViewContribution
    QDockWidget *createDockableView(
        const NativeViewContribution &contribution,
        QWidget *widget,
        QObject *viewModel = nullptr);

    // == Create Standalone Window (for Preview)
    QWidget *createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title);

    // == Apply Menu Contributions from plugins
    void applyMenuContributions(const QList<MenuContribution> &contributions);

    // == Central Widget Management ==
    /**
     * @brief Set or replace the central widget of the main window
     * 
     * @param widget The widget to set as central widget. Ownership is transferred to main_window_.
     *               If nullptr, the current central widget is removed.
     * @param isExternalWidget If true, the widget is managed externally (e.g., by a plugin).
     *                         WindowManager will not delete it during cleanup.
     * @return true if successfully set, false if main_window_ is null
     */
    bool setCentralWidget(QWidget *widget, bool isExternalWidget = false);

    /**
     * @brief Get the current central widget
     */
    QWidget *centralWidget() const;

    /**
     * @brief Take back the central widget without deleting it
     * 
     * Removes the central widget from the main window and returns it.
     * Ownership is transferred to the caller.
     */
    QWidget *takeCentralWidget();

    // == Hot Reload Support ==
    /**
     * @brief Set hot reload mode
     *
     * When enabled, QML files will be loaded from filesystem (qmlHotDir) instead of qrc resources
     */
    void setHotReloadEnabled(bool enabled);
    bool isHotReloadEnabled() const { return _hot_reload_enabled; }

    /**
     * @brief Reload all QML views
     *
     * Reload all registered QML views, used for hot reload
     */
    void reloadAllQmlViews();

private:
    QDockWidget *createDockWidgetCommon(
        const QString &viewId,
        const QString &title,
        QWidget *content,
        Qt::DockWidgetArea dockArea,
        QDockWidget::DockWidgetFeatures features,
        Qt::DockWidgetAreas allowedAreas);

    void cleanupQmlWidget(QWidget *widget);

    QMainWindow *_main_window = nullptr;
    EditorPluginManager *_plugin_mng = nullptr;
    bool _cleaned_up = false;
    bool _hot_reload_enabled = false;

    // Track externally passed widgets (tracked safely using QPointer)
    // Key: viewId, Value: weak reference to external widget
    QHash<QString, QPointer<QWidget>> _external_widgets;

    // Store QML view contribution info and ViewModel for hot reload refresh
    struct QmlViewInfo {
        ViewContribution contribution;
        QObject *viewModel;
        QPointer<QWidget> quickWidget;
    };
    QHash<QString, QmlViewInfo> _qml_views;
};

}// namespace rbc