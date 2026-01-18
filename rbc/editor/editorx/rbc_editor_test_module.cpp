/**
 * RBC Editor Testbed
 * 共享相似的启动流程，但是支持热更新调整样式
 */
#include <QWindow>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QShortcut>
#include <QStringList>
#include <QtGlobal>
#include <rbc_config.h>
#include <QQmlEngine>
#include <QDebug>
#include <QCommandLineParser>
#include <QKeySequence>
#include "RBCEditorRuntime/core/EditorEngine.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/services/LayoutService.h"

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Quick Application with Hot Reload Support");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption enableHotReloadOption(
        QStringList() << "enable_hot_reload",
        QCoreApplication::translate("main", "Enable hot reload for QML files"));
    parser.addOption(enableHotReloadOption);
    parser.process(app);
    // Get QML file path or watch directory path
    QString qmlPath;
    bool enable_hot_reload = parser.isSet(enableHotReloadOption);
    qDebug() << "Hot Reload 【" << (enable_hot_reload ? "enabled" : "disabled") << "】\n";

    EditorEngine::instance().init(argc, argv);
    auto &pluginManager = EditorPluginManager::instance();
    QQmlEngine *engine = new QQmlEngine();
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";

    // 4. Load Plugins from DLL
    if (!pluginManager.loadPluginFromDLL("RBCE_ConnectionPlugin")) {
        qWarning() << "Failed to load ConnectionPlugin";
    }
    // if (!pluginManager.loadPluginFromDLL("RBCE_ProjectPlugin")) {
    //     qWarning() << "Failed to load ProjectPlugin";
    // }
    // if (!pluginManager.loadPluginFromDLL("RBCE_NodeEditorPlugin")) {
    //     qWarning() << "Failed to load NodeEditorPlugin";
    // }

    int result = 0;
    {
        WindowManager windowManager(&pluginManager, nullptr);
        windowManager.setup_main_window();

        // 设置热更新模式
        if (enable_hot_reload) {
            windowManager.setHotReloadEnabled(true);
        }

        auto *mainWindow = windowManager.main_window();
        qDebug() << "Main window created";
        auto *layoutService = new LayoutService(&app);
        pluginManager.registerService(layoutService);
        layoutService->initialize(&windowManager, &pluginManager);
        layoutService->loadBuiltInLayouts();
        qDebug() << "LayoutService initialized";

        // 5.2 Load LayoutPlugin (depends on LayoutService)
        if (!pluginManager.loadPluginFromDLL("RBCE_LayoutPlugin")) {
            qWarning() << "Failed to load LayoutPlugin";
        }

        for (auto *plugin : pluginManager.getLoadedPlugins()) {
            qDebug() << "Processing menu contributions for plugin:" << plugin->id();
            QList<MenuContribution> menus = plugin->menu_contributions();
            if (!menus.isEmpty()) {
                windowManager.applyMenuContributions(menus);
            }
        }

        if (layoutService->hasLayout("rbce.layout.test")) {
            qDebug() << "Applying test layout...";
            layoutService->applyLayout("rbce.layout.test");
        }

        // 热更新模式：添加 F5 快捷键监听
        QShortcut *reloadShortcut = nullptr;
        if (enable_hot_reload) {
            reloadShortcut = new QShortcut(QKeySequence(Qt::Key_F5), mainWindow);
            QObject::connect(reloadShortcut, &QShortcut::activated, [&windowManager]() {
                qDebug() << "F5 pressed - Reloading all QML views...";
                windowManager.reloadAllQmlViews();
            });
            qDebug() << "Hot reload: F5 shortcut registered for QML refresh";
        }

        mainWindow->resize(1920, 1080);
        mainWindow->show();
        qDebug() << "Main window shown";
        result = app.exec();
        qDebug() << "Step 1: Cleaning up WindowManager...";
        windowManager.cleanup();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
        qDebug() << "Step 2: WindowManager scope ending, destroying all QQuickWidgets...";
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    qDebug() << "WindowManager destroyed, all QQuickWidgets deleted";
    qDebug() << "Step 3: Cleaning up QML Engine (before plugin unload)...";
    pluginManager.setQmlEngine(nullptr);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    delete engine;
    engine = nullptr;
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    qDebug() << "QML Engine cleaned up";
    qDebug() << "Step 4: Unloading all plugins...";
    pluginManager.unloadAllPlugins();
    qDebug() << "Step 5: Clearing PluginManager service references...";
    pluginManager.clearServices();
    qDebug() << "Step 6: Shutting down EditorEngine...";
    EditorEngine::instance().shutdown();
    qDebug() << "Cleanup completed, returning from dll_main...";

    return result;
}
