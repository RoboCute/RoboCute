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
#include <luisa/runtime/context.h>

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication app(argc, argv);
    QCommandLineParser parser;
    parser.setApplicationDescription("RBC Graph Testbed with Hot Reload Support");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption enableHotReloadOption(
        QStringList() << "enable_hot_reload",
        QCoreApplication::translate("main", "Enable hot reload for QML files"));
    parser.addOption(enableHotReloadOption);
    parser.process(app);
    bool enable_hot_reload = parser.isSet(enableHotReloadOption);
    qDebug() << "Hot Reload 【" << (enable_hot_reload ? "enabled" : "disabled") << "】\n";

    EditorEngine::instance().init(argc, argv);
    auto &pluginManager = EditorPluginManager::instance();
    QQmlEngine *engine = new QQmlEngine();
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";

    if (!pluginManager.loadPluginFromDLL("RBCE_NodeEditorPlugin")) {
        qWarning() << "Failed to load NodeEditorPlugin";
    }

    int result = 0;
    {
        WindowManager windowManager(&pluginManager, nullptr);
        windowManager.setup_main_window();
        if (enable_hot_reload) {
            windowManager.setHotReloadEnabled(true);
        }
        auto *mainWindow = windowManager.main_window();
        auto *layoutService = new LayoutService(&app);
        pluginManager.registerService(layoutService);
        layoutService->initialize(&windowManager, &pluginManager);
        layoutService->loadBuiltInLayouts();
        // ui/menu plugin for Layout
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
        auto startup_layout = "rbce.layout.graph_dev";
        if (layoutService->hasLayout(startup_layout)) {
            layoutService->applyLayout(startup_layout);
        }

        QShortcut *reloadShortcut = nullptr;
        if (enable_hot_reload) {
            reloadShortcut = new QShortcut(QKeySequence(Qt::Key_F5), mainWindow);
            QObject::connect(reloadShortcut, &QShortcut::activated, [&windowManager]() {
                qDebug() << "F5 pressed - Reloading all QML views...";
                windowManager.reloadAllQmlViews();
            });
            qDebug() << "Hot reload: Ctrl+F5 shortcut registered for QML refresh";
        }

        mainWindow->resize(1920, 1080);
        mainWindow->show();
        qDebug() << "Main window shown";
        result = app.exec();
        windowManager.cleanup();
        QCoreApplication::processEvents(QEventLoop::AllEvents);
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    pluginManager.setQmlEngine(nullptr);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    delete engine;
    engine = nullptr;
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    pluginManager.unloadAllPlugins();
    pluginManager.clearServices();
    EditorEngine::instance().shutdown();
    return result;
}
