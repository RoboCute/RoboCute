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

// Connection Plugin
// #include "ConnectionPlugin.h"

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;

    // 1. Initialize Render Engine Before Qt
    EditorEngine::instance().init(argc, argv);

    // 2. Create Qt Application
    // Set Qt Quick Controls style to Fusion via environment variable
    // This allows custom backgrounds in Button and TextField
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    QApplication app(argc, argv);

    // 3. Initialize Plugin System
    auto &pluginManager = EditorPluginManager::instance();
    // 4. Create and Register Core Services
    // Create ConnectionService

    auto *connectionService = new ConnectionService(&app);
    pluginManager.registerService(connectionService);
    qDebug() << "ConnectionService registered";

    // 5. Create QML Engine
    QQmlEngine *engine = new QQmlEngine(&app);
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";

    // 6. Load Built-in Plugins
    // Load ConnectionPlugin
    if (!pluginManager.loadPlugin("RBCE_ConnectionPlugin")) {
        qWarning() << "Failed to load ConnectionPlugin";
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

    for (auto *plugin : pluginManager.getLoadedPlugins()) {
        qDebug() << "Processing plugin:" << plugin->id();
        addViewContribution(plugin);
    }

    // 9. Fill unused dock areas with placeholders to keep layout stable
    auto addPlaceholder = [&](const QString &areaName, Qt::DockWidgetArea area) {
        if (occupiedDockAreas.contains(areaName)) {
            return;
        }
        auto *placeholderDock = new QDockWidget(areaName + " Placeholder", mainWindow);
        auto *label = new QLabel(QString("%1 area placeholder").arg(areaName), placeholderDock);
        label->setAlignment(Qt::AlignCenter);
        placeholderDock->setWidget(label);
        placeholderDock->setFeatures(QDockWidget::NoDockWidgetFeatures);
        if (area != Qt::NoDockWidgetArea) {
            mainWindow->addDockWidget(area, placeholderDock);
        }
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