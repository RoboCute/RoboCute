#include <QWindow>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QStringList>
#include <QtGlobal>
#include <rbc_config.h>

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    // 1. Initialize Render Engine Before Qt
    // EditorEngine::instance().init(argc, argv);

    // 2. Create Qt Application
    // QApplication app(argc, argv);

    // 3. Initialize Plugin System
    // auto& pluginManager = PluginManager::instance();

    // 4. Create and Register Core Service
    // auto *eventBus = new EventBus();
    // auto *sceneService = new SceneService(eventBus);
    // auto *styleManager = new StyleManager();
    // auto *layoutService = new LayoutService();
    // auto *resultService = new ResultService();

    // pluginManager.registerService("EventBus", eventBus);
    // pluginManager.registerService("SceneService", sceneService);
    // pluginManager.registerService("StyleManager", styleManager);
    // pluginManager.registerService("LayoutService", layoutService);
    // pluginManager.registerService("ResultService", resultService);

    // 5. Initialize Style Manager
    // styleManager->initialize(argc, argv);

    // 6. Create QML Engine
    // QQmlEngine engine;
    // pluginManager.setQmlEngine(&engine);

    // 7. Create Built-in Plugin
    // pluginManager.loadPlugin(new CorePlugin());
    // pluginManager.loadPlugin(new ViewportPlugin());
    // pluginManager.loadPlugin(new HierarchyPlugin());
    // pluginManager.loadPlugin(new DetailPlugin());
    // pluginManager.loadPlugin(new NodeEditorPlugin());
    // pluginManager.loadPlugin(new ResultViewerPlugin());

    // 8. Scan and Load Out Plugin
    // pluginManager.watchPluginDirectory("./plugins");

    // 9. Create Main Window
    // WindowManager windowManager(&pluginManager);
    // windowManager.setupMainWindow();

    // 10. Apply the UI contributions for all plugins
    // for (auto* plugin : pluginManager.getLoadedPlugins()) {
    //     for (const auto& view : plugin->viewContributions()) {
    //         // 从Plugin获取对应的ViewModel
    //         auto* viewModel = plugin->createViewModel(view.viewId);
    //         if (viewModel) {
    //             windowManager.createDockableView(view, viewModel);
    //         }
    //     }
    // }

    // 11. Restore Default Layout
    // layoutService->loadLayout("default");

    // 12. Show Main Window
    // windowManager.mainWindow()->show();

    // 13. Run Event Loop
    // int result = app.exec();

    // 14. Clear
    // pluginManager.unloadAllPlugins();
    // EditorEngine::instance().shutdown();

    // QApplication app(argc, argv);
    // QWidget w;
    // w.setWindowTitle("EditorX");
    // w.show();
    // int res = app.exec();
    // return res;
    return 0;
}