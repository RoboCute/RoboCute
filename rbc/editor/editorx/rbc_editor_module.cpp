#include <QWindow>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QLabel>
#include <QStringList>
#include <QtGlobal>
#include <QSet>
#include <rbc_config.h>
#include <QQmlEngine>
#include <QDebug>

#include "RBCEditorRuntime/core/EditorEngine.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/services/ConnectionService.h"
#include "RBCEditorRuntime/services/ProjectService.h"

#include <argparse/argparse.hpp>

// Connection Plugin
// #include "ConnectionPlugin.h"

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;
    EditorEngine::instance().init(argc, argv);
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication app(argc, argv);
    auto &pluginManager = EditorPluginManager::instance();
    auto *connectionService = new ConnectionService(&app);
    pluginManager.registerService(connectionService);
    qDebug() << "ConnectionService registered";

    // Register ProjectService
    auto *projectService = new ProjectService(&app);
    pluginManager.registerService(projectService);
    qDebug() << "ProjectService registered";

    // 5. Create QML Engine
    QQmlEngine *engine = new QQmlEngine(&app);
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";
    // 6. Load Built-in Plugins
    // Load ConnectionPlugin
    if (!pluginManager.loadPlugin("RBCE_ConnectionPlugin")) {
        qWarning() << "Failed to load ConnectionPlugin";
    }
    // Load ProjectPlugin
    if (!pluginManager.loadPlugin("RBCE_ProjectPlugin")) {
        qWarning() << "Failed to load ProjectPlugin";
    }
    // 7. Create Main Window
    WindowManager windowManager(&pluginManager, &app);
    windowManager.setup_main_window();
    auto *mainWindow = windowManager.main_window();
    qDebug() << "Main window created";
    // 8. Apply UI contributions for all plugins
    QSet<QString> occupiedDockAreas;
    auto addViewContribution = [&](IEditorPlugin *plugin) {
        if (!plugin) {
            return;
        }
        QList<ViewContribution> views = plugin->view_contributions();
        for (const auto &view : views) {
            qDebug() << "Creating view:" << view.viewId << "from" << view.qmlSource;
            QObject *viewModel = plugin->getViewModel(view.viewId);
            if (!viewModel) {
                qWarning() << "Failed to get ViewModel for view:" << view.viewId;
                continue;
            }
            QDockWidget *dock = windowManager.createDockableView(view, viewModel);
            if (dock) {
                qDebug() << "Created dock for view:" << view.viewId;
                occupiedDockAreas.insert(view.dockArea);
            } else {
                qWarning() << "Failed to create dock for view:" << view.viewId;
            }
        }
    };

    // Apply menu contributions
    for (auto *plugin : pluginManager.getLoadedPlugins()) {
        qDebug() << "Processing plugin:" << plugin->id();
        QList<MenuContribution> menus = plugin->menu_contributions();
        if (!menus.isEmpty()) {
            windowManager.applyMenuContributions(menus);
        }
    }

    // Create file browser dock for ProjectPlugin
    IEditorPlugin *projectPlugin = pluginManager.getPlugin("com.robocute.project_plugin");
    if (projectPlugin) {
        // Use QObject property to get the file browser widget
        QWidget *fileBrowserWidget = projectPlugin->property("fileBrowserWidget").value<QWidget *>();
        if (fileBrowserWidget) {
            QDockWidget *fileBrowserDock = windowManager.createDockableView(
                "project_file_browser",
                "Project Files",
                fileBrowserWidget,
                Qt::LeftDockWidgetArea,
                QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable,
                Qt::AllDockWidgetAreas);

            if (fileBrowserDock) {
                occupiedDockAreas.insert("Left");
                qDebug() << "Created file browser dock";
            }
        } else {
            qWarning() << "File browser widget not available from ProjectPlugin";
        }
    }

    // Apply view contributions
    for (auto *plugin : pluginManager.getLoadedPlugins()) {
        qDebug() << "Processing plugin:" << plugin->id();
        addViewContribution(plugin);
    }

    // 9. Fill unused dock areas with placeholders to keep layout stable
    auto addPlaceholder = [&](const QString &areaName, Qt::DockWidgetArea area) {
        if (occupiedDockAreas.contains(areaName)) {
            return;
        }
        auto *label = new QLabel(QString("%1 area placeholder").arg(areaName), nullptr);
        label->setAlignment(Qt::AlignCenter);
        windowManager.createDockableView(
            QStringLiteral("Placeholder_") + areaName,
            areaName + " Placeholder",
            label,
            area,
            QDockWidget::NoDockWidgetFeatures,
            Qt::DockWidgetAreas(area));
    };

    addPlaceholder("Left", Qt::LeftDockWidgetArea);
    addPlaceholder("Right", Qt::RightDockWidgetArea);
    addPlaceholder("Top", Qt::TopDockWidgetArea);
    addPlaceholder("Bottom", Qt::BottomDockWidgetArea);

    // central workspace placeholder
    if (!mainWindow->centralWidget()) {
        auto *centralLabel = new QLabel("Workspace placeholder", mainWindow);
        centralLabel->setAlignment(Qt::AlignCenter);
        mainWindow->setCentralWidget(centralLabel);
    }

    // 10. Show Main Window
    mainWindow->resize(1920, 1080);
    mainWindow->show();
    qDebug() << "Main window shown";

    // 11. Run Event Loop
    int result = app.exec();

    // 12. Cleanup
    pluginManager.unloadAllPlugins();
    EditorEngine::instance().shutdown();

    return result;
}