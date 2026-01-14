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
    // 重要：WindowManager 是栈上对象，不能设置 parent 为 &app
    // 否则会导致 double-delete：栈对象析构 + app 析构时删除 children
    WindowManager windowManager(&pluginManager, nullptr);
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
            // 注意：isExternalWidget=true 表示这个 widget 由 plugin 创建和管理
            // WindowManager 不会在析构时删除它，而是在 cleanup() 时释放引用
            QDockWidget *fileBrowserDock = windowManager.createDockableView(
                "project_file_browser",
                "Project Files",
                fileBrowserWidget,
                Qt::LeftDockWidgetArea,
                QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable,
                Qt::AllDockWidgetAreas,
                true /* isExternalWidget */);

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

    // ========================================================================
    // 12. Cleanup - 严格按照依赖关系逆序清理，这是关键！
    // ========================================================================
    
    // 12.1 首先清理 WindowManager：
    //      - 隐藏窗口停止渲染
    //      - 清理 QML context 引用（打破对 ViewModel 的引用）
    //      - 释放外部 widget 引用（让 plugin 管理其 widget）
    qDebug() << "Step 1: Cleaning up WindowManager...";
    windowManager.cleanup();
    
    // 12.2 然后卸载所有 plugin：
    //      - plugin 可以安全地清理自己创建的 widget（WindowManager 已经释放引用）
    //      - ViewModel 可以安全地销毁（QML 已经不再引用它）
    //      - plugin 可以断开与 service 的连接
    qDebug() << "Step 2: Unloading all plugins...";
    pluginManager.unloadAllPlugins();
    
    // 12.3 清理 PluginManager 的 service 引用：
    //      - 清空 services_ map 的引用（不调用 disconnect，避免访问已删除对象）
    //      - 这必须在 app 析构前完成！
    //      - 否则 EditorPluginManager 析构时（main 返回后）会访问已被删除的 service
    qDebug() << "Step 3: Clearing PluginManager service references...";
    pluginManager.clearServices();
    
    // 12.4 关闭 EditorEngine
    qDebug() << "Step 4: Shutting down EditorEngine...";
    EditorEngine::instance().shutdown();
    
    // 12.5 函数返回，局部变量自动析构（按创建顺序的逆序）：
    //      - windowManager 析构（简单删除 main_window_）
    //      - engine 析构
    //      - app 析构（删除 projectService, connectionService 等子对象）
    //      此时 PluginManager 的 services_ 已经被清空，不会访问已删除对象
    // 12.6 main() 返回后，静态对象析构：
    //      - EditorPluginManager::instance() 析构（services_ 已清空，安全）
    //      - EditorEngine::instance() 析构
    
    qDebug() << "Cleanup completed, returning from dll_main...";
    return result;
}