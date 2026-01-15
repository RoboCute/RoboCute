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
    for (auto it = external_widgets_.begin(); it != external_widgets_.end(); ++it) {
        QString viewId = it.key();
        QPointer<QWidget> widgetPtr = it.value();
        
        if (widgetPtr) {
            // 找到对应的 dock 并解除关联
            QDockWidget *dock = main_window_->findChild<QDockWidget *>(viewId);
            if (dock && dock->widget() == widgetPtr) {
                // 从 dock 中移除外部 widget，但不删除它
                // 这样 plugin 仍然可以安全地管理它
                dock->setWidget(nullptr);
                qDebug() << "WindowManager::cleanup: Released external widget:" << viewId;
            }
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

    // Resolve QML URL (support qrc:/ and file://)
    QUrl qmlUrl;
    if (contribution.qmlSource.startsWith("qrc:/") || contribution.qmlSource.startsWith(":/")) {
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
    
    Q_UNUSED(viewModel); // 可用于未来扩展
    
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
        contribution.isExternalManaged
    );
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

}// namespace rbc