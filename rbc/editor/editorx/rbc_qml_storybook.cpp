#include <rbc_config.h>

#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QStringList>
#include <QDebug>
#include <QUrl>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTimer>

#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>

#include "RBCEditorRuntime/core/EditorEngine.h"
#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/ui/WindowManager.h"
#include "RBCEditorRuntime/services/LayoutService.h"

#include <QQuickWidget>

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    using namespace rbc;
    
    // 1. Overall Start QApplication
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");
    QApplication app(argc, argv);
    
    // 2. Initialize EditorEngine
    EditorEngine::instance().init(argc, argv);
    auto &pluginManager = EditorPluginManager::instance();

    // 3. Create QML Engine
    // 注意：不设置 parent，由我们手动管理生命周期
    // 这样可以确保在所有 QQuickWidget 销毁后再删除 engine
    QQmlEngine *engine = new QQmlEngine();
    pluginManager.setQmlEngine(engine);
    qDebug() << "QML Engine created for Storybook";

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

    // Parse command line arguments for storybook
    QCommandLineParser parser;
    parser.setApplicationDescription("RoboCute Storybook - QML Component Testing and Hot Reload");
    parser.addHelpOption();
    
    QCommandLineOption enableHotReloadOption(
        QStringList() << "enable_hot_reload",
        QCoreApplication::translate("main", "Enable hot reload for QML files"));
    parser.addOption(enableHotReloadOption);

    QCommandLineOption qmlPathOption(
        QStringList() << "qml_path",
        QCoreApplication::translate("main", "Path to QML Root directory to watch"),
        QCoreApplication::translate("main", "qml_dir"));
    parser.addOption(qmlPathOption);
    
    parser.process(app);

    bool enable_hot_reload = parser.isSet(enableHotReloadOption);
    QString qmlPath;
    if (parser.isSet(qmlPathOption)) {
        qmlPath = parser.value(qmlPathOption);
    } else {
        // Default path is `qrc://`
        qmlPath = "qrc://";
    }
    
    qDebug() << "Storybook Hot Reload [" << (enable_hot_reload ? "enabled" : "disabled") << "]";
    qDebug() << "QML Path:" << qmlPath;

    // Setup QML path and hot reload
    QString watchDir;
    QString mainQmlFile;
    
    if (enable_hot_reload) {
        QFileInfo pathInfo(qmlPath);
        if (!pathInfo.exists()) {
            qWarning() << "QML path not found:" << qmlPath;
            qWarning() << "Usage: --enable_hot_reload --qml_path <path_to_qml_directory>";
            return -1;
        }
        qmlPath = pathInfo.absoluteFilePath();
        watchDir = qmlPath;
        QDir dir(watchDir);
        mainQmlFile = dir.absoluteFilePath("StorybookMain.qml");
        qputenv("QML_DISABLE_DISK_CACHE", "1");
        qputenv("QT_QUICK_COMPILER_DISABLE_CACHE", "1");
        
        engine->addImportPath(qmlPath);
        QFileInfo mainQmlInfo(qmlPath);
        QUrl baseUrl = QUrl::fromLocalFile(mainQmlInfo.absolutePath() + "/");
        engine->setBaseUrl(baseUrl);
    } else {
        mainQmlFile = "qrc://qml/StorybookMain.qml";
    }

    int result = 0;

    // ========================================================================
    // WindowManager 作用域块
    // ========================================================================
    {
        // 5. Create Main Window
        WindowManager windowManager(&pluginManager, nullptr);
        windowManager.setup_main_window();
        auto *mainWindow = windowManager.main_window();
        qDebug() << "Storybook main window created";

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

        // 6. Create Storybook UI
        // Create a central widget with component selector and preview area
        QWidget *centralWidget = new QWidget(mainWindow);
        QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
        
        // Left panel: Component list
        QWidget *leftPanel = new QWidget(centralWidget);
        QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
        
        QLabel *listLabel = new QLabel("Available Components", leftPanel);
        QListWidget *componentList = new QListWidget(leftPanel);
        QPushButton *reloadButton = new QPushButton("Reload Components", leftPanel);
        QPushButton *selectQmlPathButton = new QPushButton("Select QML Path", leftPanel);
        
        leftLayout->addWidget(listLabel);
        leftLayout->addWidget(componentList);
        leftLayout->addWidget(reloadButton);
        leftLayout->addWidget(selectQmlPathButton);
        leftPanel->setLayout(leftLayout);
        leftPanel->setMinimumWidth(250);
        
        // Right panel: Component preview
        QWidget *rightPanel = new QWidget(centralWidget);
        QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
        
        QLabel *previewLabel = new QLabel("Component Preview", rightPanel);
        QQuickWidget *previewWidget = new QQuickWidget(engine, rightPanel);
        previewWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        
        rightLayout->addWidget(previewLabel);
        rightLayout->addWidget(previewWidget);
        rightPanel->setLayout(rightLayout);
        
        mainLayout->addWidget(leftPanel);
        mainLayout->addWidget(rightPanel, 1); // Right panel takes more space
        centralWidget->setLayout(mainLayout);
        
        mainWindow->setCentralWidget(centralWidget);
        
        // Store component list and preview widget for lambda capture
        QListWidget *componentListPtr = componentList;
        QQuickWidget *previewWidgetPtr = previewWidget;
        QString *currentQmlPathPtr = new QString(qmlPath);
        QString *watchDirPtr = new QString(watchDir);
        bool *hotReloadEnabledPtr = new bool(enable_hot_reload);
        
        // Function to scan and load QML components
        auto scanQmlComponents = [=]() {
            componentListPtr->clear();
            QString searchPath = *currentQmlPathPtr;
            
            if (searchPath.startsWith("qrc://")) {
                // For qrc resources, we can't scan dynamically
                // Add some default components
                componentListPtr->addItem("StorybookMain.qml");
            } else {
                // Scan directory for QML files
                QDir dir(searchPath);
                if (dir.exists()) {
                    QDirIterator it(searchPath, QStringList() << "*.qml", QDir::Files, QDirIterator::Subdirectories);
                    while (it.hasNext()) {
                        QString filePath = it.next();
                        QFileInfo fileInfo(filePath);
                        if (fileInfo.isFile()) {
                            QString relativePath = dir.relativeFilePath(filePath);
                            componentListPtr->addItem(relativePath);
                        }
                    }
                }
            }
        };
        
        // Function to load component using QQmlComponent (Component form)
        // This uses Component form: QQmlComponent loads QML files as components,
        // which can then be instantiated. This is the recommended way for plugin QML.
        auto loadComponent = [=](const QString &componentPath) {
            qDebug() << "Loading component:" << componentPath;
            
            QUrl componentUrl;
            if (componentPath.startsWith("qrc://") || componentPath.startsWith(":/")) {
                componentUrl = QUrl(componentPath);
            } else if (componentPath.startsWith("file://")) {
                componentUrl = QUrl(componentPath);
            } else {
                // Relative path - resolve based on current QML path
                QString basePath = *currentQmlPathPtr;
                if (basePath.startsWith("qrc://")) {
                    componentUrl = QUrl(basePath + componentPath);
                } else {
                    QDir dir(basePath);
                    QString fullPath = dir.absoluteFilePath(componentPath);
                    componentUrl = QUrl::fromLocalFile(fullPath);
                }
            }
            
            // Use QQmlComponent to load the component (Component form)
            // This is the Component form: we use QQmlComponent to load QML files
            // as reusable components, which aligns with the requirement that
            // "all QML should be used in Component form"
            QQmlComponent component(engine, componentUrl);
            
            // Wait for component to be ready (if asynchronous)
            if (component.isLoading()) {
                QEventLoop loop;
                QObject::connect(&component, &QQmlComponent::statusChanged, &loop, &QEventLoop::quit);
                loop.exec();
            }
            
            if (component.isError()) {
                qWarning() << "Failed to load component:" << componentUrl;
                for (const QQmlError &error : component.errors()) {
                    qWarning() << "  Error:" << error.toString();
                }
                return;
            }
            
            // Create context for the component instance
            QQmlContext *context = new QQmlContext(engine->rootContext(), previewWidgetPtr);
            // Set any context properties if needed
            // context->setContextProperty("viewModel", someViewModel);
            
            // Create the component instance using Component.create()
            // This demonstrates Component form usage: QQmlComponent.create()
            QObject *object = component.create(context);
            if (!object) {
                qWarning() << "Failed to create component instance from:" << componentUrl;
                if (component.isError()) {
                    for (const QQmlError &error : component.errors()) {
                        qWarning() << "  Error:" << error.toString();
                    }
                }
                delete context;
                return;
            }
            
            // Set the component instance as context property for potential use in QML
            previewWidgetPtr->rootContext()->setContextProperty("componentInstance", object);
            
            // Load the QML file in the preview widget
            // Note: If the QML file is a Component definition, it needs a wrapper
            // For regular QML files (Item, Rectangle, etc.), this works directly
            previewWidgetPtr->setSource(componentUrl);
            
            // Reparent object to previewWidget for proper cleanup
            object->setParent(previewWidgetPtr);
            delete context;
            
            if (previewWidgetPtr->status() == QQuickWidget::Error) {
                qWarning() << "Failed to load component in preview widget:" << componentUrl;
                for (const QQmlError &error : previewWidgetPtr->errors()) {
                    qWarning() << "  Error:" << error.toString();
                }
            } else {
                qDebug() << "Component loaded successfully using Component form (QQmlComponent):" << componentUrl;
            }
        };
        
        // Connect component list selection
        QObject::connect(componentList, &QListWidget::itemDoubleClicked, 
            [=](QListWidgetItem *item) {
                QString componentPath = item->text();
                loadComponent(componentPath);
            });
        
        // Connect reload button
        QObject::connect(reloadButton, &QPushButton::clicked, [=]() {
            scanQmlComponents();
            if (componentListPtr->count() > 0) {
                componentListPtr->setCurrentRow(0);
                loadComponent(componentListPtr->currentItem()->text());
            }
        });
        
        // Connect select QML path button
        QObject::connect(selectQmlPathButton, &QPushButton::clicked, [=]() {
            QString dir = QFileDialog::getExistingDirectory(mainWindow, 
                "Select QML Directory", *currentQmlPathPtr);
            if (!dir.isEmpty()) {
                *currentQmlPathPtr = dir;
                *watchDirPtr = dir;
                engine->addImportPath(dir);
                QFileInfo dirInfo(dir);
                QUrl baseUrl = QUrl::fromLocalFile(dirInfo.absolutePath() + "/");
                engine->setBaseUrl(baseUrl);
                scanQmlComponents();
            }
        });
        
        // Initial scan
        scanQmlComponents();
        
        // Setup hot reload if enabled
        QFileSystemWatcher *watcher = nullptr;
        QTimer *reloadTimer = nullptr;
        
        if (enable_hot_reload) {
            watcher = new QFileSystemWatcher(&app);
            
            // Recursively find all QML files
            QDir dir(*watchDirPtr);
            QStringList qmlFiles;
            QDirIterator it(*watchDirPtr, QStringList() << "*.qml", QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString filePath = it.next();
                QFileInfo fileInfo(filePath);
                if (fileInfo.isFile()) {
                    watcher->addPath(filePath);
                    qmlFiles << filePath;
                }
            }
            watcher->addPath(*watchDirPtr);
            
            qDebug() << "Watching" << qmlFiles.size() << "QML files in directory:" << *watchDirPtr;
            
            // Debounce timer for reload
            reloadTimer = new QTimer(&app);
            reloadTimer->setSingleShot(true);
            reloadTimer->setInterval(200);
            
            auto doReload = [=]() {
                qDebug() << "QML file changed, reloading component...";
                QString currentComponent = componentListPtr->currentItem() 
                    ? componentListPtr->currentItem()->text() 
                    : QString();
                
                if (!currentComponent.isEmpty()) {
                    // Clear component cache
                    engine->clearComponentCache();
                    engine->trimComponentCache();
                    
                    // Reload the current component
                    loadComponent(currentComponent);
                }
            };
            
            QObject::connect(reloadTimer, &QTimer::timeout, doReload);
            
            auto reloadQml = [reloadTimer]() {
                if (reloadTimer->isActive()) {
                    reloadTimer->stop();
                }
                reloadTimer->start();
            };
            
            // Watch file changes
            QObject::connect(watcher, &QFileSystemWatcher::fileChanged,
                [watcher, watchDirPtr, reloadQml](const QString &path) {
                    if (path.endsWith(".qml", Qt::CaseInsensitive)) {
                        qDebug() << "QML file changed:" << path;
                        QTimer::singleShot(100, [watcher, path]() {
                            if (!watcher->files().contains(path)) {
                                watcher->addPath(path);
                            }
                        });
                        reloadQml();
                    }
                });
            
            // Watch directory changes
            QObject::connect(watcher, &QFileSystemWatcher::directoryChanged,
                [watcher, watchDirPtr, reloadQml, scanQmlComponents]() {
                    qDebug() << "Directory changed:" << *watchDirPtr;
                    QTimer::singleShot(100, [watcher, watchDirPtr]() {
                        if (!watcher->directories().contains(*watchDirPtr)) {
                            watcher->addPath(*watchDirPtr);
                        }
                    });
                    scanQmlComponents();
                    reloadQml();
                });
        }

        // 8. Show Main Window
        mainWindow->resize(1920, 1080);
        mainWindow->setWindowTitle("RoboCute Storybook");
        mainWindow->show();
        qDebug() << "Storybook main window shown";

        // 9. Run Event Loop
        result = app.exec();

        // ====================================================================
        // 10. Cleanup Phase 1 - WindowManager 作用域内清理
        // ====================================================================
        qDebug() << "Step 1: Cleaning up WindowManager...";
        windowManager.cleanup();

        QCoreApplication::processEvents(QEventLoop::AllEvents);

        qDebug() << "Step 2: WindowManager scope ending, destroying all QQuickWidgets...";
    }

    // ========================================================================
    // 11. Cleanup Phase 2 - WindowManager 作用域外清理
    // ========================================================================
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
