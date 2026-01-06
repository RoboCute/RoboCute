#include <QWindow>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QStringList>
#include <QtGlobal>
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
    QApplication app(argc, argv);

    // 3. Initialize Plugin System
    auto &pluginManager = EditorPluginManager::instance();

    // 4. Create and Register Core Services
    // Create ConnectionService
    auto *connectionService = new ConnectionService(&app);
    pluginManager.registerService("ConnectionService", connectionService);
    qDebug() << "ConnectionService registered";

    // 5. Create QML Engine
    QQmlEngine *engine = new QQmlEngine(&app);
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";

    // 6. Load Built-in Plugins
    // Load ConnectionPlugin
    pluginManager.loadPlugin("RBCE_ConnectionPlugin");

    // // 7. Create Main Window
    // WindowManager windowManager(&pluginManager, &app);
    // windowManager.setup_main_window();
    // qDebug() << "Main window created";

    // // 8. Apply UI contributions for all plugins
    // for (auto *plugin : pluginManager.getLoadedPlugins()) {
    //     qDebug() << "Processing plugin:" << plugin->id();

    //     // Get view contributions
    //     QList<ViewContribution> views = plugin->view_contributions();
    //     for (const auto &view : views) {
    //         qDebug() << "Creating view:" << view.viewId << "from" << view.qmlSource;

    //         // Get ViewModel from plugin
    //         QObject *viewModel = plugin->getViewModel(view.viewId);
    //         if (!viewModel) {
    //             qWarning() << "Failed to get ViewModel for view:" << view.viewId;
    //             continue;
    //         }

    //         // Create dockable view
    //         QDockWidget *dock = windowManager.createDockableView(view, viewModel);
    //         if (dock) {
    //             qDebug() << "Created dock for view:" << view.viewId;
    //         } else {
    //             qWarning() << "Failed to create dock for view:" << view.viewId;
    //         }
    //     }
    // }

    // // 9. Show Main Window
    // windowManager.main_window()->resize(1280, 720);
    // windowManager.main_window()->show();
    // qDebug() << "Main window shown";
    // // 10. Run Event Loop
    // int result = app.exec();

    // 11. Cleanup
    pluginManager.unloadAllPlugins();
    EditorEngine::instance().shutdown();

    // return result;
    return 0;
}