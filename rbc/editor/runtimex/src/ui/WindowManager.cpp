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
    : QObject(parent), main_window_(nullptr), plugin_mng_(plugin_mng) {
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
    Qt::DockWidgetAreas allowedAreas) {

    return createDockWidgetCommon(viewId, title, widget, dockArea, features, allowedAreas);
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