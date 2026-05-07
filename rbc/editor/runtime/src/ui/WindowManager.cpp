#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include <QQuickWidget>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QDebug>
#include <QUrl>
#include <QStringList>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QKeySequence>
#include <QCoreApplication>
#include <QEventLoop>
#include <functional>

namespace rbc {

static Qt::DockWidgetArea parse_dock_area(const QString &dockArea) {
    if (dockArea == "Right") {
        return Qt::RightDockWidgetArea;
    } else if (dockArea == "Top") {
        return Qt::TopDockWidgetArea;
    } else if (dockArea == "Bottom") {
        return Qt::BottomDockWidgetArea;
    } else if (dockArea == "Center") {
        return Qt::NoDockWidgetArea;// Center is not a dock area
    }
    return Qt::LeftDockWidgetArea;
}

WindowManager::WindowManager(EditorPluginManager *plugin_mng, QObject *parent)
    : QObject(parent), _plugin_mng(plugin_mng) {
}

WindowManager::~WindowManager() {
    // Ensure cleanup() has been called
    if (!_cleaned_up) {
        qWarning() << "WindowManager::~WindowManager: cleanup() was not called before destruction!";
        cleanup();
    }

    // Simple destructor: relies on Qt's parent-child auto-cleanup mechanism
    // _main_window and all child widgets are auto-deleted
    if (_main_window) {
        delete _main_window;
        _main_window = nullptr;
    }

    qDebug() << "WindowManager::~WindowManager: Destroyed";
}

void WindowManager::cleanupQmlWidget(QWidget *widget) {
    QQuickWidget *quickWidget = qobject_cast<QQuickWidget *>(widget);
    if (!quickWidget) return;

    // Hide QQuickWidget to stop rendering
    quickWidget->hide();

    // Clean up context properties, break references to ViewModel
    // This is key: prevent QML from accessing ViewModel after destruction
    QQmlContext *context = quickWidget->rootContext();
    if (context) {
        context->setContextProperty("viewModel", nullptr);
    }

    // Clear source to stop QML execution
    quickWidget->setSource(QUrl());

    qDebug() << "WindowManager::cleanupQmlWidget: Cleaned up QQuickWidget";
}

void WindowManager::cleanup() {
    if (_cleaned_up) {
        return;
    }
    _cleaned_up = true;

    if (!_main_window) {
        return;
    }

    qDebug() << "WindowManager::cleanup: Starting cleanup...";

    // 1. Hide window, stop all rendering and event processing
    _main_window->hide();

    // 2. Clean up QML widgets in all dock widgets
    //    This breaks QML's reference to ViewModel, preventing access to destroyed objects after plugin unload
    QList<QDockWidget *> dockWidgets = _main_window->findChildren<QDockWidget *>();
    for (QDockWidget *dock : dockWidgets) {
        QWidget *widget = dock->widget();
        if (widget) {
            cleanupQmlWidget(widget);
        }
    }

    // 3. Release external widget references
    //    External widgets are managed by their creator (plugin), we just release references
    //    Key: must remove external widgets from dock before WindowManager destruction
    //    Otherwise dock deletion will also delete external widgets, causing plugin double-free
    for (auto it = _external_widgets.begin(); it != _external_widgets.end(); ++it) {
        QString viewId = it.key();
        QPointer<QWidget> widgetPtr = it.value();

        if (widgetPtr) {
            // Find corresponding dock and disassociate
            QDockWidget *dock = _main_window->findChild<QDockWidget *>(viewId);
            if (dock) {
                QWidget *dockWidget = dock->widget();
                if (dockWidget == widgetPtr) {
                    // Remove external widget from dock but do not delete it
                    // So plugin can still safely manage it
                    dock->setWidget(nullptr);
                    widgetPtr->setParent(nullptr);// Ensure complete detachment from parent-child relationship
                    qDebug() << "WindowManager::cleanup: Released external widget:" << viewId;
                } else {
                    // Widget may be wrapped in other containers
                    // Try to remove directly from parent
                    qWarning() << "WindowManager::cleanup: Widget mismatch for" << viewId
                               << "- dock->widget() is different, forcing release";
                    widgetPtr->setParent(nullptr);// Force detach from parent
                    qDebug() << "WindowManager::cleanup: Force-released external widget:" << viewId;
                }
            } else {
                // Dock not found, widget may be central widget
                // Check if it is central widget
                if (_main_window->centralWidget() == widgetPtr ||
                    widgetPtr->parent() == _main_window->centralWidget()) {
                    widgetPtr->setParent(nullptr);
                    qDebug() << "WindowManager::cleanup: Released external central widget:" << viewId;
                } else {
                    // Cannot find dock, detach directly from parent
                    qWarning() << "WindowManager::cleanup: Dock not found for" << viewId
                               << "- forcing release from parent";
                    widgetPtr->setParent(nullptr);
                }
            }
        } else {
            qDebug() << "WindowManager::cleanup: External widget already deleted:" << viewId;
        }
    }
    _external_widgets.clear();

    // 4. Disconnect menu action signal connections
    //    Menu callbacks may capture plugin object pointers, need to disconnect before plugin unload
    QMenuBar *menuBar = _main_window->menuBar();
    if (menuBar) {
        std::function<void(QMenu *)> disconnectMenuActions;
        disconnectMenuActions = [&disconnectMenuActions](QMenu *menu) {
            if (!menu) return;
            for (QAction *action : menu->actions()) {
                QObject::disconnect(action, &QAction::triggered, nullptr, nullptr);
                if (action->menu()) {
                    disconnectMenuActions(action->menu());
                }
            }
        };

        for (QAction *action : menuBar->actions()) {
            if (action->menu()) {
                disconnectMenuActions(action->menu());
            }
        }
    }

    qDebug() << "WindowManager::cleanup: Cleanup completed";
}

void WindowManager::setup_main_window() {
    if (!_main_window) {
        _main_window = new QMainWindow();
        _main_window->setWindowTitle("RoboCute Editor");
    }
}

QDockWidget *WindowManager::createDockWidgetCommon(
    const QString &viewId,
    const QString &title,
    QWidget *content,
    Qt::DockWidgetArea dockArea,
    QDockWidget::DockWidgetFeatures features,
    Qt::DockWidgetAreas allowedAreas) {

    if (!_main_window) {
        qWarning() << "WindowManager::createDockWidgetCommon: _main_window is null, call setup_main_window() first";
        return nullptr;
    }
    if (!content) {
        qWarning() << "WindowManager::createDockWidgetCommon: content widget is null";
        return nullptr;
    }

    QDockWidget *dock = new QDockWidget(title, _main_window);
    dock->setObjectName(viewId);
    dock->setAllowedAreas(allowedAreas);
    dock->setFeatures(features);
    dock->setWidget(content);

    if (dockArea != Qt::NoDockWidgetArea) {
        _main_window->addDockWidget(dockArea, dock);
    }
    return dock;
}

QDockWidget *WindowManager::createDockableView(const ViewContribution &contribution, QObject *viewModel) {
    if (!_main_window) {
        qWarning() << "WindowManager::createDockableView: _main_window is null, call setup_main_window() first";
        return nullptr;
    }

    if (!_plugin_mng || !_plugin_mng->qmlEngine()) {
        qWarning() << "WindowManager::createDockableView: QML engine is not available";
        return nullptr;
    }

    QQmlEngine *engine = _plugin_mng->qmlEngine();

    // Create QQuickWidget
    QQuickWidget *quickWidget = new QQuickWidget(engine, nullptr);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Resolve QML URL (support qrc:/, file://, or hot reload from filesystem)
    QUrl qmlUrl;

    if (_hot_reload_enabled && !contribution.qmlHotDir.isEmpty()) {
        // Hot-reload mode: load from filesystem
        QString filePath = contribution.qmlHotDir + "/qml/" + contribution.qmlSource;
        qmlUrl = QUrl::fromLocalFile(filePath);
        qDebug() << "WindowManager: Hot reload mode - loading QML from:" << filePath;
    } else if (contribution.qmlSource.startsWith("qrc:/") || contribution.qmlSource.startsWith(":/")) {
        qmlUrl = QUrl(contribution.qmlSource);
    } else if (contribution.qmlSource.startsWith("file://")) {
        qmlUrl = QUrl(contribution.qmlSource);
    } else {
        // Assume it's a relative path from qrc:/qml/
        qmlUrl = QUrl("qrc:/qml/" + contribution.qmlSource);
    }

    // Set root context property for ViewModel (before loading QML)
    QQmlContext *context = quickWidget->rootContext();
    context->setContextProperty("viewModel", viewModel);

    // Load QML
    quickWidget->setSource(qmlUrl);

    if (quickWidget->status() == QQuickWidget::Error) {
        qWarning() << "WindowManager::createDockableView: Failed to load QML:" << qmlUrl;
        qWarning() << "Errors:" << quickWidget->errors();
        delete quickWidget;
        return nullptr;
    }

    // Parse dock area
    Qt::DockWidgetArea dockArea = parse_dock_area(contribution.dockArea);

    // Parse preferred size
    if (!contribution.preferredSize.isEmpty()) {
        QStringList sizeParts = contribution.preferredSize.split(",");
        if (sizeParts.size() == 2) {
            bool ok1, ok2;
            int width = sizeParts[0].trimmed().toInt(&ok1);
            int height = sizeParts[1].trimmed().toInt(&ok2);
            if (ok1 && ok2) {
                quickWidget->setMinimumSize(width, height);
            }
        }
    }

    QDockWidget *dock = createDockWidgetCommon(
        contribution.viewId,
        contribution.title,
        quickWidget,
        dockArea,
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable,
        Qt::AllDockWidgetAreas);
    if (!dock) {
        delete quickWidget;
        return nullptr;
    }

    // Store QML view info for hot-reload refresh
    QmlViewInfo viewInfo;
    viewInfo.contribution = contribution;
    viewInfo.viewModel = viewModel;
    viewInfo.quickWidget = quickWidget;
    _qml_views.insert(contribution.viewId, viewInfo);

    qDebug() << "WindowManager::createDockableView: Created dock for" << contribution.viewId;
    return dock;
}

QDockWidget *WindowManager::createDockableView(
    const QString &viewId,
    const QString &title,
    QWidget *widget,
    Qt::DockWidgetArea dockArea,
    QDockWidget::DockWidgetFeatures features,
    Qt::DockWidgetAreas allowedAreas,
    bool isExternalWidget) {

    QDockWidget *dock = createDockWidgetCommon(viewId, title, widget, dockArea, features, allowedAreas);

    // If external widget, track it with QPointer
    // So references can be safely released in cleanup(), letting plugin manage its lifecycle
    if (dock && isExternalWidget) {
        _external_widgets.insert(viewId, QPointer<QWidget>(widget));
        qDebug() << "WindowManager::createDockableView: Registered external widget:" << viewId;
    }

    return dock;
}

QDockWidget *WindowManager::createDockableView(
    const NativeViewContribution &contribution,
    QWidget *widget,
    QObject *viewModel) {

    Q_UNUSED(viewModel);// Can be used for future extensions

    Qt::DockWidgetArea area = parse_dock_area(contribution.dockArea);

    QDockWidget::DockWidgetFeatures features = QDockWidget::NoDockWidgetFeatures;
    if (contribution.closable)
        features |= QDockWidget::DockWidgetClosable;
    if (contribution.movable)
        features |= QDockWidget::DockWidgetMovable;
    if (contribution.floatable)
        features |= QDockWidget::DockWidgetFloatable;

    return createDockableView(
        contribution.viewId,
        contribution.title,
        widget,
        area,
        features,
        Qt::AllDockWidgetAreas,
        contribution.isExternalManaged);
}

QWidget *WindowManager::createStandaloneView(const QString &qmlSource, QObject *viewModel, const QString &title) {
    if (!_plugin_mng || !_plugin_mng->qmlEngine()) {
        qWarning() << "WindowManager::createStandaloneView: QML engine is not available";
        return nullptr;
    }

    QQmlEngine *engine = _plugin_mng->qmlEngine();

    // Create QQuickWidget
    QQuickWidget *quickWidget = new QQuickWidget(engine, nullptr);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->setWindowTitle(title);

    // Resolve QML URL
    QUrl qmlUrl;
    if (qmlSource.startsWith("qrc:/") || qmlSource.startsWith(":/")) {
        qmlUrl = QUrl(qmlSource);
    } else if (qmlSource.startsWith("file://")) {
        qmlUrl = QUrl(qmlSource);
    } else {
        qmlUrl = QUrl("qrc:/qml/" + qmlSource);
    }

    // Set root context property
    QQmlContext *context = quickWidget->rootContext();
    context->setContextProperty("viewModel", viewModel);

    // Load QML
    quickWidget->setSource(qmlUrl);

    if (quickWidget->status() == QQuickWidget::Error) {
        qWarning() << "WindowManager::createStandaloneView: Failed to load QML:" << qmlUrl;
        qWarning() << "Errors:" << quickWidget->errors();
        delete quickWidget;
        return nullptr;
    }

    return quickWidget;
}

bool WindowManager::setCentralWidget(QWidget *widget, bool isExternalWidget) {
    if (!_main_window) {
        qWarning() << "WindowManager::setCentralWidget: _main_window is null";
        return false;
    }

    // If there's an existing central widget that's external, untrack it
    QWidget *oldCentral = _main_window->centralWidget();
    if (oldCentral) {
        // Check if old central widget was in _external_widgets
        QString oldViewId;
        for (auto it = _external_widgets.begin(); it != _external_widgets.end(); ++it) {
            if (it.value() == oldCentral) {
                oldViewId = it.key();
                break;
            }
        }
        if (!oldViewId.isEmpty()) {
            _external_widgets.remove(oldViewId);
            qDebug() << "WindowManager::setCentralWidget: Removed old external central widget:" << oldViewId;
        }
    }

    // Set the new central widget
    _main_window->setCentralWidget(widget);

    // Track if it's an external widget
    if (widget && isExternalWidget) {
        // Use the widget's objectName or a special key for tracking
        QString viewId = widget->objectName();
        if (viewId.isEmpty()) {
            viewId = QStringLiteral("__central_widget__");
        }
        _external_widgets.insert(viewId, QPointer<QWidget>(widget));
        qDebug() << "WindowManager::setCentralWidget: Registered external central widget:" << viewId;
    }

    qDebug() << "WindowManager::setCentralWidget: Central widget set successfully";
    return true;
}

QWidget *WindowManager::centralWidget() const {
    if (!_main_window) {
        return nullptr;
    }
    return _main_window->centralWidget();
}

QWidget *WindowManager::takeCentralWidget() {
    if (!_main_window) {
        return nullptr;
    }
    return _main_window->takeCentralWidget();
}

void WindowManager::applyMenuContributions(const QList<MenuContribution> &contributions) {
    if (!_main_window) {
        qWarning() << "WindowManager::applyMenuContributions: _main_window is null";
        return;
    }

    QMenuBar *menuBar = _main_window->menuBar();
    if (!menuBar) {
        menuBar = new QMenuBar(_main_window);
        _main_window->setMenuBar(menuBar);
    }

    for (const auto &contribution : contributions) {
        // Parse menu path (e.g., "File/Open Project")
        QStringList pathParts = contribution.menuPath.split("/");
        if (pathParts.isEmpty()) {
            qWarning() << "WindowManager::applyMenuContributions: Empty menu path";
            continue;
        }

        // Find or create menu hierarchy
        QMenu *currentMenu = nullptr;
        QString menuName = pathParts.first();

        // Find existing menu or create new one
        QList<QAction *> menuBarActions = menuBar->actions();
        for (QAction *action : menuBarActions) {
            if (action->text() == menuName) {
                currentMenu = action->menu();
                break;
            }
        }

        if (!currentMenu) {
            currentMenu = menuBar->addMenu(menuName);
        }

        // Navigate/create submenus if needed
        // If pathParts has more than one element, the last one is the action text
        // Otherwise, use contribution.actionText
        QString actionText;
        if (pathParts.size() > 1) {
            // Navigate through submenus
            for (int i = 1; i < pathParts.size() - 1; ++i) {
                QString subMenuName = pathParts[i];
                QMenu *subMenu = nullptr;

                QList<QAction *> menuActions = currentMenu->actions();
                for (QAction *action : menuActions) {
                    if (action->text() == subMenuName && action->menu()) {
                        subMenu = action->menu();
                        break;
                    }
                }

                if (!subMenu) {
                    subMenu = currentMenu->addMenu(subMenuName);
                }
                currentMenu = subMenu;
            }
            // Last part is the action text
            actionText = pathParts.last();
        } else {
            // Single level menu, use actionText from contribution
            actionText = contribution.actionText;
        }

        if (actionText.isEmpty()) {
            qWarning() << "WindowManager::applyMenuContributions: Empty action text for" << contribution.menuPath;
            continue;
        }

        QAction *action = currentMenu->addAction(actionText);
        if (!contribution.shortcut.isEmpty()) {
            action->setShortcut(QKeySequence(contribution.shortcut));
        }
        action->setObjectName(contribution.actionId);

        // Connect callback
        if (contribution.callback) {
            QObject::connect(action, &QAction::triggered, contribution.callback);
        }

        qDebug() << "WindowManager::applyMenuContributions: Added menu item" << contribution.menuPath;
    }
}

void WindowManager::setHotReloadEnabled(bool enabled) {
    _hot_reload_enabled = enabled;
    qDebug() << "WindowManager: Hot reload" << (enabled ? "enabled" : "disabled");
}

void WindowManager::reloadAllQmlViews() {
    if (!_plugin_mng || !_plugin_mng->qmlEngine()) {
        qWarning() << "WindowManager::reloadAllQmlViews: QML engine is not available";
        return;
    }

    qDebug() << "WindowManager::reloadAllQmlViews: Reloading" << _qml_views.size() << "QML views...";

    QQmlEngine *engine = _plugin_mng->qmlEngine();

    for (auto it = _qml_views.begin(); it != _qml_views.end(); ++it) {
        const QString &viewId = it.key();
        QmlViewInfo &viewInfo = it.value();

        if (!viewInfo.quickWidget) {
            qWarning() << "WindowManager::reloadAllQmlViews: QQuickWidget for" << viewId << "has been deleted";
            continue;
        }

        QQuickWidget *quickWidget = qobject_cast<QQuickWidget *>(viewInfo.quickWidget.data());
        if (!quickWidget) {
            qWarning() << "WindowManager::reloadAllQmlViews: Widget is not QQuickWidget for" << viewId;
            continue;
        }

        // Recalculate QML URL
        QUrl qmlUrl;
        const ViewContribution &contribution = viewInfo.contribution;

        if (_hot_reload_enabled && !contribution.qmlHotDir.isEmpty()) {
            QString filePath = contribution.qmlHotDir + "/qml/" + contribution.qmlSource;
            qmlUrl = QUrl::fromLocalFile(filePath);
            qDebug() << "WindowManager::reloadAllQmlViews: Reloading" << viewId << "from: " << filePath;
        } else if (contribution.qmlSource.startsWith("qrc:/") || contribution.qmlSource.startsWith(":/")) {
            qmlUrl = QUrl(contribution.qmlSource);
        } else if (contribution.qmlSource.startsWith("file://")) {
            qmlUrl = QUrl(contribution.qmlSource);
        } else {
            qmlUrl = QUrl("qrc:/qml/" + contribution.qmlSource);
        }

        // Re-set ViewModel (ensure context binding is correct)
        QQmlContext *context = quickWidget->rootContext();
        if (context && viewInfo.viewModel) {
            context->setContextProperty("viewModel", viewInfo.viewModel);
        }

        // Reload QML
        // 1. First clear source to ensure complete unloading of old content
        quickWidget->setSource(QUrl());
        engine->clearComponentCache();
        engine->trimComponentCache();
        // 2. Process events to ensure unloading completes
        QCoreApplication::processEvents();

        // 3. Reload new QML
        qDebug() << "WindowManager::reloadAllQmlViews: Setting source to:" << qmlUrl.toString();
        quickWidget->setSource(qmlUrl);

        if (quickWidget->status() == QQuickWidget::Error) {
            qWarning() << "WindowManager::reloadAllQmlViews: Failed to reload QML for" << viewId;
            qWarning() << "Errors:" << quickWidget->errors();
        } else {
            qDebug() << "WindowManager::reloadAllQmlViews: Successfully reloaded" << viewId;
        }
    }

    // Process all pending events to ensure cache cleanup completes
    QCoreApplication::processEvents();

    qDebug() << "WindowManager::reloadAllQmlViews: Reload completed";
}

}// namespace rbc