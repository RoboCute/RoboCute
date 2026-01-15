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

#include <argparse/argparse.hpp>

// Connection Plugin
// #include "ConnectionPlugin.h"

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;
    // 1. Overall Start QApplication
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication app(argc, argv);
    // 2. Initalize EditorEngine

    EditorEngine::instance().init(argc, argv);
    auto &pluginManager = EditorPluginManager::instance();
    // 3. Create QML Engine
    QQmlEngine *engine = new QQmlEngine(&app);
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created";
    // 4. Load Plugins from DLL
    if (!pluginManager.loadPluginFromDLL("RBCE_ConnectionPlugin")) {
        qWarning() << "Failed to load ConnectionPlugin";
    }
    if (!pluginManager.loadPluginFromDLL("RBCE_ProjectPlugin")) {
        qWarning() << "Failed to load ProjectPlugin";
    }
    // 5. Create Main Window
    WindowManager windowManager(&pluginManager, nullptr);
    windowManager.setup_main_window();
    auto *mainWindow = windowManager.main_window();
    qDebug() << "Main window created";
    // 6. Apply UI contributions for all plugins
    // 6.1 Apply QML View Contribution
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
    // 6.2 Apply Native View Contribution
    auto addNativeViewContribution = [&](IEditorPlugin *plugin) {
        if (!plugin) {
            return;
        }
        QList<NativeViewContribution> nativeViews = plugin->native_view_contributions();
        for (const auto &nativeView : nativeViews) {
            qDebug() << "Creating native view:" << nativeView.viewId << "with title" << nativeView.title;
            QWidget *widget = plugin->getNativeWidget(nativeView.viewId);
            if (!widget) {
                qWarning() << "Failed to get native widget for view:" << nativeView.viewId;
                continue;
            }
            QObject *viewModel = plugin->getViewModel(nativeView.viewId);
            QDockWidget *dock = windowManager.createDockableView(nativeView, widget, viewModel);
            if (dock) {
                qDebug() << "Created native dock for view:" << nativeView.viewId;
                occupiedDockAreas.insert(nativeView.dockArea);
            } else {
                qWarning() << "Failed to create native dock for view:" << nativeView.viewId;
            }
        }
    };

    // 6.3 Apply menu contributions
    for (auto *plugin : pluginManager.getLoadedPlugins()) {
        qDebug() << "Processing plugin:" << plugin->id();
        QList<MenuContribution> menus = plugin->menu_contributions();
        if (!menus.isEmpty()) {
            windowManager.applyMenuContributions(menus);
        }
    }

    // 6.4 Apply native view contributions
    for (auto *plugin : pluginManager.getLoadedPlugins()) {
        qDebug() << "Processing native views for plugin:" << plugin->id();
        addNativeViewContribution(plugin);
    }

    // 6.5 Apply view contributions
    for (auto *plugin : pluginManager.getLoadedPlugins()) {
        qDebug() << "Processing plugin:" << plugin->id();
        addViewContribution(plugin);
    }

    // 7. Fill unused dock areas with placeholders to keep layout stable
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

    // 8. Show Main Window
    mainWindow->resize(1920, 1080);
    mainWindow->show();
    qDebug() << "Main window shown";

    // 9. Run Event Loop
    int result = app.exec();

    // ========================================================================
    // 10. Cleanup - 严格按照依赖关系逆序清理，这是关键！
    // ========================================================================

    // 10.1 首先清理 WindowManager：
    //      - 隐藏窗口停止渲染
    //      - 清理 QML context 引用（打破对 ViewModel 的引用）
    //      - 释放外部 widget 引用（让 plugin 管理其 widget）
    qDebug() << "Step 1: Cleaning up WindowManager...";
    windowManager.cleanup();

    // 10.2 然后卸载所有 plugin：
    //      - plugin 可以安全地清理自己创建的 widget（WindowManager 已经释放引用）
    //      - ViewModel 可以安全地销毁（QML 已经不再引用它）
    //      - plugin 可以断开与 service 的连接
    qDebug() << "Step 2: Unloading all plugins...";
    pluginManager.unloadAllPlugins();

    // 10.3 清理 PluginManager 的 service 引用：
    //      - 清空 services_ map 的引用（不调用 disconnect，避免访问已删除对象）
    //      - 这必须在 app 析构前完成！
    //      - 否则 EditorPluginManager 析构时（main 返回后）会访问已被删除的 service
    qDebug() << "Step 3: Clearing PluginManager service references...";
    pluginManager.clearServices();

    // 10.4 关闭 EditorEngine
    qDebug() << "Step 4: Shutting down EditorEngine...";
    EditorEngine::instance().shutdown();
    qDebug() << "Cleanup completed, returning from dll_main...";
    return result;
}