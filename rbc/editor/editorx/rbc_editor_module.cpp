#include <QWindow>
#include <QGuiApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QStringList>
#include <QtGlobal>
#include <rbc_config.h>
#include <QQmlEngine>
#include <QDebug>

#include "RBCEditorRuntime/core/EditorEngine.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/services/LayoutService.h"

#include <argparse/argparse.hpp>

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;
    // 1. Overall Start QApplication
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication app(argc, argv);
    // 2. Initalize EditorEngine

    EditorEngine::instance().init(argc, argv);
    auto &pluginManager = EditorPluginManager::instance();

    // 3. Create QML Engine
    // 注意：不设置 parent，由我们手动管理生命周期
    // 这样可以确保在所有 QQuickWidget 销毁后再删除 engine
    QQmlEngine *engine = new QQmlEngine();
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";

    // 4. Load Plugins from DLL
    if (!pluginManager.loadPluginFromDLL("RBCE_ConnectionPlugin")) {
        qWarning() << "Failed to load ConnectionPlugin";
    }
    if (!pluginManager.loadPluginFromDLL("RBCE_ProjectPlugin")) {
        qWarning() << "Failed to load ProjectPlugin";
    }
    if (!pluginManager.loadPluginFromDLL("RBCE_NodeEditorPlugin")) {
        qWarning() << "Failed to load NodeEditorPlugin";
    }

    int result = 0;

    // ========================================================================
    // WindowManager 作用域块
    // ========================================================================
    // 使用作用域块来精确控制 WindowManager 的生命周期
    // 确保所有 QQuickWidget 在 QQmlEngine 删除之前被销毁
    {
        // 5. Create Main Window
        WindowManager windowManager(&pluginManager, nullptr);
        windowManager.setup_main_window();
        auto *mainWindow = windowManager.main_window();
        qDebug() << "Main window created";

        // 5.1 Create and initialize LayoutService
        auto *layoutService = new LayoutService(&app);
        pluginManager.registerService(layoutService);
        layoutService->initialize(&windowManager, &pluginManager);
        layoutService->loadBuiltInLayouts();
        qDebug() << "LayoutService initialized";

        // 5.2 Load LayoutPlugin (depends on LayoutService)
        if (!pluginManager.loadPluginFromDLL("RBCE_LayoutPlugin")) {
            qWarning() << "Failed to load LayoutPlugin";
        }

        // 6. Apply UI contributions for all plugins
        // 6.1 Apply menu contributions (menus are not managed by LayoutService)
        for (auto *plugin : pluginManager.getLoadedPlugins()) {
            qDebug() << "Processing menu contributions for plugin:" << plugin->id();
            QList<MenuContribution> menus = plugin->menu_contributions();
            if (!menus.isEmpty()) {
                windowManager.applyMenuContributions(menus);
            }
        }

        // 7. Apply the default layout - LayoutService manages all DockArea layouts
        // LayoutService will create dock widgets for views defined in the layout config
        // If a viewId is not found in plugins, it will create a placeholder
        if (layoutService->hasLayout("scene_editing")) {
            qDebug() << "Applying scene_editing layout...";
            layoutService->applyLayout("scene_editing");
        } else {
            qWarning() << "scene_editing layout not found, using placeholder for central widget";
            // Fallback: central workspace placeholder
            if (!mainWindow->centralWidget()) {
                auto *centralLabel = new QLabel("Workspace placeholder", mainWindow);
                centralLabel->setAlignment(Qt::AlignCenter);
                mainWindow->setCentralWidget(centralLabel);
            }
        }

        // 8. Show Main Window
        mainWindow->resize(1920, 1080);
        mainWindow->show();
        qDebug() << "Main window shown";

        // 9. Run Event Loop
        result = app.exec();

        // ====================================================================
        // 10. Cleanup Phase 1 - WindowManager 作用域内清理
        // ====================================================================
        //
        // QML 生命周期要点：
        // 1. QQuickWidget 内部持有 QQmlContext，QQmlContext 引用 QQmlEngine
        // 2. QQmlEngine 内部持有 QJSEngine，QJSEngine 管理 JavaScript 对象
        // 3. 删除 QQmlEngine 时，QJSEngine 会清理所有 JS 对象
        // 4. 如果此时还有 QQuickWidget 存在，它们的 QQmlContext 可能仍在使用 engine
        // 5. 这会导致 atomic lockRelaxed 读取访问冲突
        //
        // 正确的清理顺序：
        // 1. 清理 WindowManager（隐藏窗口、清理 context properties）
        // 2. 销毁 WindowManager（作用域结束时，删除所有 QQuickWidget）
        // 3. 然后在作用域外：卸载 plugins、删除 QQmlEngine

        // 10.1 首先清理 WindowManager：
        //  - 隐藏窗口停止渲染
        //  - 清理 QML context 引用（打破对 ViewModel 的引用）
        //  - 释放外部 widget 引用（让 plugin 管理其 widget）
        qDebug() << "Step 1: Cleaning up WindowManager...";
        windowManager.cleanup();

        // 处理所有待处理事件
        QCoreApplication::processEvents(QEventLoop::AllEvents);

        qDebug() << "Step 2: WindowManager scope ending, destroying all QQuickWidgets...";
        // WindowManager 析构函数将在这里被调用，删除 main_window_ 及其所有子 widget
        // 包括所有 QQuickWidget，确保它们在 QQmlEngine 删除之前被销毁
    }

    // ========================================================================
    // 11. Cleanup Phase 2 - WindowManager 作用域外清理
    // ========================================================================
    //
    // 关键：QML 类型注册与 DLL 卸载的顺序
    // ---------------------------------------------------------------
    // 插件通过 qmlRegisterType 注册 QML 类型到 QQmlEngine
    // 这些类型的元信息（vtable、静态函数指针等）存储在插件 DLL 中
    //
    // 如果先卸载插件（DLL 被卸载），再删除 QQmlEngine：
    // - QQmlEngine 析构时会清理已注册的类型
    // - 此时类型的元信息已经失效（DLL 已卸载）
    // - 访问失效的内存导致 atomic lockRelaxed 访问冲突
    //
    // 正确顺序：
    // 1. 删除 QQmlEngine（DLL 仍然加载，类型信息有效）
    // 2. 然后卸载插件（DLL 可以安全卸载）

    // 处理事件，确保所有 deferred deletion 完成
    QCoreApplication::processEvents(QEventLoop::AllEvents);
    qDebug() << "WindowManager destroyed, all QQuickWidgets deleted";

    // 11.1 清理 QML 引擎引用和显式删除引擎：
    //      - 必须在插件卸载之前删除 engine！
    //      - 此时所有 QQuickWidget 已经被销毁
    //      - DLL 仍然加载，qmlRegisterType 注册的类型信息仍然有效
    //      - engine 可以安全地清理这些类型
    qDebug() << "Step 3: Cleaning up QML Engine (before plugin unload)...";
    pluginManager.setQmlEngine(nullptr);

    // 处理所有待处理事件，确保所有 QML 相关的清理都已完成
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // 显式删除 QML 引擎
    delete engine;
    engine = nullptr;

    // 再次处理事件，确保引擎删除完成
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    qDebug() << "QML Engine cleaned up";

    // 11.2 卸载所有 plugin：
    //      - QQmlEngine 已删除，不会再访问插件注册的 QML 类型
    //      - plugin 可以安全地清理自己创建的资源
    //      - ViewModel 可以安全地销毁（QML widget 已经不存在，engine 已删除）
    //      - DLL 可以安全卸载
    qDebug() << "Step 4: Unloading all plugins...";
    pluginManager.unloadAllPlugins();

    // 11.3 清理 PluginManager 的 service 引用：
    //      - 清空 services_ map 的引用（不调用 disconnect，避免访问已删除对象）
    //      - 这必须在 app 析构前完成！
    //      - 否则 EditorPluginManager 析构时（main 返回后）会访问已被删除的 service
    qDebug() << "Step 5: Clearing PluginManager service references...";
    pluginManager.clearServices();

    // 11.4 关闭 EditorEngine
    qDebug() << "Step 6: Shutting down EditorEngine...";
    EditorEngine::instance().shutdown();
    qDebug() << "Cleanup completed, returning from dll_main...";

    return result;
}
