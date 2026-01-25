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
    : QObject(parent), plugin_mng_(plugin_mng) {
}

WindowManager::~WindowManager() {
    // 确保 cleanup() 已被调用
    if (!cleaned_up_) {
        qWarning() << "WindowManager::~WindowManager: cleanup() was not called before destruction!";
        cleanup();
    }

    // 简单的析构：依赖 Qt 的 parent-child 自动清理机制
    // main_window_ 及其所有子 widget 会被自动删除
    if (main_window_) {
        delete main_window_;
        main_window_ = nullptr;
    }

    qDebug() << "WindowManager::~WindowManager: Destroyed";
}

void WindowManager::cleanupQmlWidget(QWidget *widget) {
    QQuickWidget *quickWidget = qobject_cast<QQuickWidget *>(widget);
    if (!quickWidget) return;

    // 隐藏 QQuickWidget 停止渲染
    quickWidget->hide();

    // 清理 context properties，打破对 ViewModel 的引用
    // 这是关键：防止 QML 在 ViewModel 销毁后访问它
    QQmlContext *context = quickWidget->rootContext();
    if (context) {
        context->setContextProperty("viewModel", nullptr);
    }

    // 清空 source 停止 QML 执行
    quickWidget->setSource(QUrl());

    qDebug() << "WindowManager::cleanupQmlWidget: Cleaned up QQuickWidget";
}

void WindowManager::cleanup() {
    if (cleaned_up_) {
        return;
    }
    cleaned_up_ = true;

    if (!main_window_) {
        return;
    }

    qDebug() << "WindowManager::cleanup: Starting cleanup...";

    // 1. 隐藏窗口，停止所有渲染和事件处理
    main_window_->hide();

    // 2. 清理所有 dock widget 中的 QML widget
    //    这一步打破 QML 对 ViewModel 的引用，防止 plugin unload 后访问已销毁对象
    QList<QDockWidget *> dockWidgets = main_window_->findChildren<QDockWidget *>();
    for (QDockWidget *dock : dockWidgets) {
        QWidget *widget = dock->widget();
        if (widget) {
            cleanupQmlWidget(widget);
        }
    }

    // 3. 释放外部 widget 的引用
    //    外部 widget 由其创建者（plugin）管理，我们只是解除引用
    //    关键：必须在 WindowManager 析构前将外部 widget 从 dock 中移除
    //    否则 dock 删除时会一起删除外部 widget，导致 plugin 双重释放
    for (auto it = external_widgets_.begin(); it != external_widgets_.end(); ++it) {
        QString viewId = it.key();
        QPointer<QWidget> widgetPtr = it.value();

        if (widgetPtr) {
            // 找到对应的 dock 并解除关联
            QDockWidget *dock = main_window_->findChild<QDockWidget *>(viewId);
            if (dock) {
                QWidget *dockWidget = dock->widget();
                if (dockWidget == widgetPtr) {
                    // 从 dock 中移除外部 widget，但不删除它
                    // 这样 plugin 仍然可以安全地管理它
                    dock->setWidget(nullptr);
                    widgetPtr->setParent(nullptr);// 确保完全脱离 parent-child 关系
                    qDebug() << "WindowManager::cleanup: Released external widget:" << viewId;
                } else {
                    // widget 可能被包装在其他容器中
                    // 尝试直接从 parent 中移除
                    qWarning() << "WindowManager::cleanup: Widget mismatch for" << viewId
                               << "- dock->widget() is different, forcing release";
                    widgetPtr->setParent(nullptr);// 强制脱离 parent
                    qDebug() << "WindowManager::cleanup: Force-released external widget:" << viewId;
                }
            } else {
                // Dock 未找到，可能 widget 是 central widget
                // 检查是否是 central widget
                if (main_window_->centralWidget() == widgetPtr ||
                    widgetPtr->parent() == main_window_->centralWidget()) {
                    widgetPtr->setParent(nullptr);
                    qDebug() << "WindowManager::cleanup: Released external central widget:" << viewId;
                } else {
                    // 无法找到 dock，直接从 parent 脱离
                    qWarning() << "WindowManager::cleanup: Dock not found for" << viewId
                               << "- forcing release from parent";
                    widgetPtr->setParent(nullptr);
                }
            }
        } else {
            qDebug() << "WindowManager::cleanup: External widget already deleted:" << viewId;
        }
    }
    external_widgets_.clear();

    // 4. 断开菜单 action 的信号连接
    //    菜单 callback 可能捕获了 plugin 对象的指针，需要在 plugin unload 前断开
    QMenuBar *menuBar = main_window_->menuBar();
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
    if (!main_window_) {
        main_window_ = new QMainWindow();
        main_window_->setWindowTitle("RoboCute Editor");
    }
}

QDockWidget *WindowManager::createDockWidgetCommon(
    const QString &viewId,
    const QString &title,
    QWidget *content,
    Qt::DockWidgetArea dockArea,
    QDockWidget::DockWidgetFeatures features,
    Qt::DockWidgetAreas allowedAreas) {

    if (!main_window_) {
        qWarning() << "WindowManager::createDockWidgetCommon: main_window_ is null, call setup_main_window() first";
        return nullptr;
    }
    if (!content) {
        qWarning() << "WindowManager::createDockWidgetCommon: content widget is null";
        return nullptr;
    }

    QDockWidget *dock = new QDockWidget(title, main_window_);
    dock->setObjectName(viewId);
    dock->setAllowedAreas(allowedAreas);
    dock->setFeatures(features);
    dock->setWidget(content);

    if (dockArea != Qt::NoDockWidgetArea) {
        main_window_->addDockWidget(dockArea, dock);
    }
    return dock;
}

QDockWidget *WindowManager::createDockableView(const ViewContribution &contribution, QObject *viewModel) {
    if (!main_window_) {
        qWarning() << "WindowManager::createDockableView: main_window_ is null, call setup_main_window() first";
        return nullptr;
    }

    if (!plugin_mng_ || !plugin_mng_->qmlEngine()) {
        qWarning() << "WindowManager::createDockableView: QML engine is not available";
        return nullptr;
    }

    QQmlEngine *engine = plugin_mng_->qmlEngine();

    // Create QQuickWidget
    QQuickWidget *quickWidget = new QQuickWidget(engine, nullptr);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    // Resolve QML URL (support qrc:/, file://, or hot reload from filesystem)
    QUrl qmlUrl;

    if (hot_reload_enabled_ && !contribution.qmlHotDir.isEmpty()) {
        // 热更新模式：从文件系统加载
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

    // 存储 QML 视图信息用于热更新刷新
    QmlViewInfo viewInfo;
    viewInfo.contribution = contribution;
    viewInfo.viewModel = viewModel;
    viewInfo.quickWidget = quickWidget;
    qml_views_.insert(contribution.viewId, viewInfo);

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

    // 如果是外部 widget，使用 QPointer 追踪它
    // 这样在 cleanup() 时可以安全地释放引用，让 plugin 管理其生命周期
    if (dock && isExternalWidget) {
        external_widgets_.insert(viewId, QPointer<QWidget>(widget));
        qDebug() << "WindowManager::createDockableView: Registered external widget:" << viewId;
    }

    return dock;
}

QDockWidget *WindowManager::createDockableView(
    const NativeViewContribution &contribution,
    QWidget *widget,
    QObject *viewModel) {

    Q_UNUSED(viewModel);// 可用于未来扩展

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
    if (!plugin_mng_ || !plugin_mng_->qmlEngine()) {
        qWarning() << "WindowManager::createStandaloneView: QML engine is not available";
        return nullptr;
    }

    QQmlEngine *engine = plugin_mng_->qmlEngine();

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
    if (!main_window_) {
        qWarning() << "WindowManager::setCentralWidget: main_window_ is null";
        return false;
    }

    // If there's an existing central widget that's external, untrack it
    QWidget *oldCentral = main_window_->centralWidget();
    if (oldCentral) {
        // Check if old central widget was in external_widgets_
        QString oldViewId;
        for (auto it = external_widgets_.begin(); it != external_widgets_.end(); ++it) {
            if (it.value() == oldCentral) {
                oldViewId = it.key();
                break;
            }
        }
        if (!oldViewId.isEmpty()) {
            external_widgets_.remove(oldViewId);
            qDebug() << "WindowManager::setCentralWidget: Removed old external central widget:" << oldViewId;
        }
    }

    // Set the new central widget
    main_window_->setCentralWidget(widget);

    // Track if it's an external widget
    if (widget && isExternalWidget) {
        // Use the widget's objectName or a special key for tracking
        QString viewId = widget->objectName();
        if (viewId.isEmpty()) {
            viewId = QStringLiteral("__central_widget__");
        }
        external_widgets_.insert(viewId, QPointer<QWidget>(widget));
        qDebug() << "WindowManager::setCentralWidget: Registered external central widget:" << viewId;
    }

    qDebug() << "WindowManager::setCentralWidget: Central widget set successfully";
    return true;
}

QWidget *WindowManager::centralWidget() const {
    if (!main_window_) {
        return nullptr;
    }
    return main_window_->centralWidget();
}

QWidget *WindowManager::takeCentralWidget() {
    if (!main_window_) {
        return nullptr;
    }
    return main_window_->takeCentralWidget();
}

void WindowManager::applyMenuContributions(const QList<MenuContribution> &contributions) {
    if (!main_window_) {
        qWarning() << "WindowManager::applyMenuContributions: main_window_ is null";
        return;
    }

    QMenuBar *menuBar = main_window_->menuBar();
    if (!menuBar) {
        menuBar = new QMenuBar(main_window_);
        main_window_->setMenuBar(menuBar);
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
    hot_reload_enabled_ = enabled;
    qDebug() << "WindowManager: Hot reload" << (enabled ? "enabled" : "disabled");
}

void WindowManager::reloadAllQmlViews() {
    if (!plugin_mng_ || !plugin_mng_->qmlEngine()) {
        qWarning() << "WindowManager::reloadAllQmlViews: QML engine is not available";
        return;
    }

    qDebug() << "WindowManager::reloadAllQmlViews: Reloading" << qml_views_.size() << "QML views...";

    QQmlEngine *engine = plugin_mng_->qmlEngine();

    for (auto it = qml_views_.begin(); it != qml_views_.end(); ++it) {
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

        // 重新计算 QML URL
        QUrl qmlUrl;
        const ViewContribution &contribution = viewInfo.contribution;

        if (hot_reload_enabled_ && !contribution.qmlHotDir.isEmpty()) {
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

        // 重新设置 ViewModel（确保 context 绑定正确）
        QQmlContext *context = quickWidget->rootContext();
        if (context && viewInfo.viewModel) {
            context->setContextProperty("viewModel", viewInfo.viewModel);
        }

        // 重新加载 QML
        // 1. 先清空 source 以确保完全卸载旧内容
        quickWidget->setSource(QUrl());
        engine->clearComponentCache();
        engine->trimComponentCache();
        // 2. 处理事件确保卸载完成
        QCoreApplication::processEvents();

        // 3. 重新加载新的 QML
        qDebug() << "WindowManager::reloadAllQmlViews: Setting source to:" << qmlUrl.toString();
        quickWidget->setSource(qmlUrl);

        if (quickWidget->status() == QQuickWidget::Error) {
            qWarning() << "WindowManager::reloadAllQmlViews: Failed to reload QML for" << viewId;
            qWarning() << "Errors:" << quickWidget->errors();
        } else {
            qDebug() << "WindowManager::reloadAllQmlViews: Successfully reloaded" << viewId;
        }
    }

    // 处理所有待处理事件，确保缓存清理完成
    QCoreApplication::processEvents();

    qDebug() << "WindowManager::reloadAllQmlViews: Reload completed";
}

}// namespace rbc