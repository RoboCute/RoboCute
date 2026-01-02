#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QDir>
#include <QDirIterator>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>
#include <QFileSystemWatcher>
#include <QQuickWindow>
#include <QWindow>
#include <QQuickStyle>

#include "RBCEditorRuntime/qml/QmlTypes.h"
#include "RBCEditorRuntime/qml/ConnectionService.h"
#include "RBCEditorRuntime/qml/EditorService.h"

// Enable QML debugging when QT_QML_DEBUG is defined (Debug builds)
#ifdef QT_QML_DEBUG
#include <QQmlDebuggingEnabler>
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

// 日志文件指针（全局，用于同时输出到控制台和文件）
static QFile* g_logFile = nullptr;
static QTextStream* g_logStream = nullptr;

// 应用配置结构
struct AppConfig {
    QString qmlDir;            // QML目录路径
    QString qmlFile;           // QML主文件路径（相对于qmlDir或绝对路径）
    bool enableHotReload = true; // 默认启用热重载（文件系统模式）
};

// 查找QML目录的默认路径
QString findDefaultQmlDir() {
    // 获取可执行文件目录（不依赖QApplication）
    QString appDirPath;
    #ifdef _WIN32
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(NULL, path, MAX_PATH)) {
            QFileInfo fileInfo(QString::fromWCharArray(path));
            appDirPath = fileInfo.absolutePath();
        }
    #else
        // Linux/Mac: 使用 /proc/self/exe 或 argv[0]
        appDirPath = QDir::currentPath();
    #endif
    
    // 尝试多个可能的路径
    QStringList possiblePaths = {
        QDir::currentPath() + "/rbc/editor/editor_next/qml",
        QDir::currentPath() + "/qml",
    };
    
    // 如果获取到了可执行文件目录，添加相关路径
    if (!appDirPath.isEmpty()) {
        possiblePaths << appDirPath + "/qml";
        possiblePaths << appDirPath + "/../qml";
        possiblePaths << appDirPath + "/../../rbc/editor/editor_next/qml";
    }
    
    for (const QString &path : possiblePaths) {
        QString mainWindowPath = path + "/MainWindow.qml";
        if (QFile::exists(mainWindowPath)) {
            return path;
        }
    }
    
    // 如果都找不到，返回当前目录下的默认路径
    return QDir::currentPath() + "/rbc/editor/editor_next/qml";
}

// 解析命令行参数
AppConfig parseArguments(int argc, char *argv[]) {
    AppConfig config;
    
    // 默认QML目录和文件（文件系统路径）
    config.qmlDir = findDefaultQmlDir();
    config.qmlFile = "MainWindow.qml";
    
    for (int i = 1; i < argc; ++i) {
        QString arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            qDebug() << "Usage:" << argv[0] << "[OPTIONS]";
            qDebug() << "Options:";
            qDebug() << "  -d, --qml-dir DIR      Set QML directory path (default: auto-detect)";
            qDebug() << "  -f, --qml-file FILE    Set QML main file name (default: MainWindow.qml)";
            qDebug() << "  --no-hot-reload        Disable hot reload";
            qDebug() << "  -h, --help             Show this help message";
            qDebug() << "";
            qDebug() << "Examples:";
            qDebug() << "  " << argv[0] << "                                    # Use auto-detected QML directory";
            qDebug() << "  " << argv[0] << " -d ./qml                          # Specify QML directory";
            qDebug() << "  " << argv[0] << " -d ./qml -f CustomWindow.qml      # Specify directory and file";
            exit(0);
        } else if (arg == "-d" || arg == "--qml-dir") {
            if (i + 1 < argc) {
                config.qmlDir = argv[++i];
            } else {
                qCritical() << "Error: --qml-dir requires a directory path argument";
                exit(1);
            }
        } else if (arg == "-f" || arg == "--qml-file") {
            if (i + 1 < argc) {
                config.qmlFile = argv[++i];
            } else {
                qCritical() << "Error: --qml-file requires a file name argument";
                exit(1);
            }
        } else if (arg == "--no-hot-reload") {
            config.enableHotReload = false;
        } else {
            qWarning() << "Unknown option:" << arg;
            qWarning() << "Use --help for usage information";
        }
    }
    
    return config;
}

// 自定义消息处理器，同时输出到控制台和文件
void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString typeStr;
    switch (type) {
        case QtDebugMsg:
            typeStr = "DEBUG";
            break;
        case QtInfoMsg:
            typeStr = "INFO";
            break;
        case QtWarningMsg:
            typeStr = "WARNING";
            break;
        case QtCriticalMsg:
            typeStr = "CRITICAL";
            break;
        case QtFatalMsg:
            typeStr = "FATAL";
            break;
    }
    
    QString logMessage = QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(typeStr)
        .arg(msg);
    
    // 如果有文件位置信息，也输出
    if (context.file) {
        logMessage += QString(" (%1:%2, %3)")
            .arg(context.file)
            .arg(context.line)
            .arg(context.function ? context.function : "");
    }
    
    // 输出到控制台
    fprintf(stderr, "%s\n", logMessage.toLocal8Bit().constData());
    fflush(stderr);
    
    // 输出到文件
    if (g_logStream) {
        *g_logStream << logMessage << Qt::endl;
        g_logStream->flush();
    }
    
    // 如果是致命错误，终止程序
    if (type == QtFatalMsg) {
        abort();
    }
}

int main(int argc, char *argv[]) {
    // 解析命令行参数
    AppConfig config = parseArguments(argc, argv);
    
    // 启用高 DPI 支持
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("RoboCute Editor");
    app.setApplicationVersion("0.3.0-next");
    
    // 设置Qt Quick Controls 2样式为非native样式，支持自定义background和contentItem
    // 使用Basic样式，它完全支持自定义，适合深色主题
    QQuickStyle::setStyle("Basic");
    qDebug() << "Qt Quick Controls 2 style set to: Basic";

#ifdef _WIN32
    // 分配控制台窗口用于调试输出
    AllocConsole();
    FILE *pCout, *pCerr, *pCin;
    freopen_s(&pCout, "CONOUT$", "w", stdout);
    freopen_s(&pCerr, "CONOUT$", "w", stderr);
    freopen_s(&pCin, "CONIN$", "r", stdin);
    SetConsoleTitleA("RoboCute Editor - Debug Console");
#endif

    // 设置日志文件路径
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(logDir);
    QString logFilePath = logDir + "/rbc_editor_next_" + 
                          QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss") + ".log";
    
    // 打开日志文件
    g_logFile = new QFile(logFilePath);
    if (g_logFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        g_logStream = new QTextStream(g_logFile);
        qDebug() << "Log file opened:" << logFilePath;
    } else {
        qWarning() << "Failed to open log file:" << logFilePath;
    }
    
    // 安装自定义消息处理器
    qInstallMessageHandler(messageHandler);
    
    qDebug() << "========================================";
    qDebug() << "RoboCute Editor Next Starting...";
    qDebug() << "Log file:" << logFilePath;
    qDebug() << "========================================";

#ifdef QT_QML_DEBUG
    // Enable QML debugging service
    // This allows Qt Creator to connect and debug QML code
    QQmlDebuggingEnabler enabler;
    qDebug() << "QML Debugging enabled (port: 3768)";
    qDebug() << "You can now connect Qt Creator's QML debugger";
#endif

    try {
        qDebug() << "QML directory:" << config.qmlDir;
        qDebug() << "QML main file:" << config.qmlFile;
        qDebug() << "Hot reload enabled:" << config.enableHotReload;
        
        // 验证QML目录是否存在
        QDir qmlDir(config.qmlDir);
        if (!qmlDir.exists()) {
            qCritical() << "QML directory not found:" << config.qmlDir;
            qCritical() << "Please specify a valid QML directory with --qml-dir";
            return -1;
        }
        
        // 构建完整的QML文件路径
        QString qmlFilePath = QDir(config.qmlDir).absoluteFilePath(config.qmlFile);
        if (!QFile::exists(qmlFilePath)) {
            qCritical() << "QML file not found:" << qmlFilePath;
            qCritical() << "Please check the QML directory and file name";
            return -1;
        }
        
        qDebug() << "Full QML file path:" << qmlFilePath;
        
        // 注册 QML 类型
        qDebug() << "Registering QML types...";
        rbc::registerQmlTypes();
        qDebug() << "QML types registered successfully";

        // 创建 QML 引擎
        QQmlApplicationEngine *engine = new QQmlApplicationEngine(&app);
        
        // 添加 QML 导入路径（文件系统路径，直接从文件系统读取，不使用资源文件或qmldir）
        QString qmlDirPath = QDir(config.qmlDir).absolutePath();
        engine->addImportPath(qmlDirPath);
        // 添加components目录作为导入路径，支持直接导入组件（不使用qmldir）
        QString componentsDirPath = QDir(config.qmlDir).absoluteFilePath("components");
        if (QDir(componentsDirPath).exists()) {
            engine->addImportPath(componentsDirPath);
        }
        qDebug() << "QML import paths (file system only, no qrc/qmldir):" << engine->importPathList();
        qDebug() << "QML directory (file system):" << qmlDirPath;
        qDebug() << "Components directory (file system):" << componentsDirPath;

        // 存储错误列表（用于在加载后检查）
        QList<QQmlError> qmlErrors;
        
        // 连接 QML 错误和警告信号
        QObject::connect(engine, &QQmlApplicationEngine::warnings, 
            [&qmlErrors](const QList<QQmlError> &warnings) {
                qWarning() << "QML Warnings/Errors (" << warnings.size() << "):";
                for (const auto &error : warnings) {
                    qWarning() << "  -" << error.toString();
                    if (!error.url().isEmpty()) {
                        qWarning() << "    URL:" << error.url().toString();
                    }
                    if (error.line() > 0) {
                        qWarning() << "    Line:" << error.line() << "Column:" << error.column();
                    }
                    qWarning() << "    Description:" << error.description();
                }
                // 保存错误以便后续检查
                qmlErrors.append(warnings);
            });

        // 创建全局服务实例（可选，也可以通过 QML 创建）
        qDebug() << "Creating service instances...";
        auto *editorService = new rbc::EditorService(&app);
        auto *connectionService = new rbc::ConnectionService(&app);
        qDebug() << "Service instances created";

        // 将服务注册为上下文属性（全局访问）
        engine->rootContext()->setContextProperty("globalEditorService", editorService);
        engine->rootContext()->setContextProperty("globalConnectionService", connectionService);
        qDebug() << "Context properties registered";

        // 使用文件系统路径
        QUrl qmlFileUrl = QUrl::fromLocalFile(qmlFilePath);
        qDebug() << "Using file system QML:" << qmlFileUrl.toString();

        // 热重载功能：文件系统监视器（默认启用）
        QFileSystemWatcher *fileWatcher = nullptr;
        QTimer *reloadTimer = nullptr; // 防抖定时器
        if (config.enableHotReload) {
            fileWatcher = new QFileSystemWatcher(&app);
            reloadTimer = new QTimer(&app);
            reloadTimer->setSingleShot(true);
            reloadTimer->setInterval(300); // 300ms防抖延迟
            
            // 监视整个QML目录及其子目录
            QDir qmlDir(config.qmlDir);
            fileWatcher->addPath(config.qmlDir);
            
            // 递归添加所有子目录和QML文件
            QDirIterator dirIt(config.qmlDir, QDirIterator::Subdirectories);
            QStringList qmlFiles;
            while (dirIt.hasNext()) {
                QString path = dirIt.next();
                QFileInfo fileInfo(path);
                if (fileInfo.isDir() && !fileInfo.isSymLink()) {
                    fileWatcher->addPath(path);
                } else if (fileInfo.isFile() && (path.endsWith(".qml") || path.endsWith(".qmldir"))) {
                    // 添加所有QML文件到监视列表
                    fileWatcher->addPath(path);
                    qmlFiles << path;
                }
            }
            
            // 确保主QML文件也在监视列表中
            if (!fileWatcher->files().contains(qmlFilePath)) {
                fileWatcher->addPath(qmlFilePath);
            }
            
            qDebug() << "Hot reload enabled for QML directory:" << config.qmlDir;
            qDebug() << "Watching" << fileWatcher->directories().size() << "directories and" 
                     << fileWatcher->files().size() << "files";
            qDebug() << "QML files being watched:" << qmlFiles.size();
            // 列出所有被监视的QML文件（用于调试）
            for (const QString &file : qmlFiles) {
                qDebug() << "  -" << file;
            }
            
            // 定义重新加载函数
            // 根据修改的文件类型决定重载策略：
            // - 主文件：需要重新加载整个窗口
            // - 子组件：只需清除缓存，QML引擎会自动重新解析
            auto reloadQml = [engine, qmlFileUrl, qmlFilePath, &qmlErrors](const QString& changedFile) {
                qDebug() << "========================================";
                qDebug() << "Hot reload: File changed:" << changedFile;
                
                // 判断是否是主文件
                bool isMainFile = (changedFile == qmlFilePath || 
                                   QFileInfo(changedFile).fileName() == QFileInfo(qmlFilePath).fileName());
                
                if (isMainFile) {
                    qDebug() << "Main file changed, reloading entire window...";
                    
                    // 主文件改变：需要重新加载整个窗口
                    QList<QObject*> oldRootObjects = engine->rootObjects();
                    qDebug() << "Current root objects count:" << oldRootObjects.size();
                    
                    // 先隐藏旧窗口（不关闭，避免程序退出）
                    for (QObject* obj : oldRootObjects) {
                        QQuickWindow* quickWindow = qobject_cast<QQuickWindow*>(obj);
                        if (quickWindow) {
                            quickWindow->setVisible(false);
                        } else {
                            QVariant visibleProp = obj->property("visible");
                            if (visibleProp.isValid()) {
                                obj->setProperty("visible", false);
                            }
                        }
                    }
                    
                    // 清除组件缓存
                    engine->clearComponentCache();
                    qDebug() << "Component cache cleared";
                    
                    // 清除错误列表
                    qmlErrors.clear();
                    
                    // 删除旧根对象
                    for (QObject* obj : oldRootObjects) {
                        obj->deleteLater();
                    }
                    
                    // 处理事件，确保旧对象被删除
                    QApplication::processEvents();
                    
                    // 重新加载QML文件
                    engine->load(qmlFileUrl);
                    QApplication::processEvents();
                    
                    // 确保新窗口可见
                    QList<QObject*> newRootObjects = engine->rootObjects();
                    for (QObject* obj : newRootObjects) {
                        QQuickWindow* quickWindow = qobject_cast<QQuickWindow*>(obj);
                        if (quickWindow) {
                            quickWindow->show();
                            quickWindow->raise();
                            quickWindow->requestActivate();
                        } else {
                            QVariant visibleProp = obj->property("visible");
                            if (visibleProp.isValid()) {
                                obj->setProperty("visible", true);
                            }
                        }
                    }
                    
                    qDebug() << "Main file reloaded, new root objects:" << newRootObjects.size();
                } else {
                    qDebug() << "Component file changed, reloading main window to refresh components...";
                    
                    // 子组件改变：需要重新加载主文件来刷新组件
                    // 因为QML引擎不会自动更新已创建的组件实例
                    QList<QObject*> oldRootObjects = engine->rootObjects();
                    qDebug() << "Current root objects count:" << oldRootObjects.size();
                    
                    // 保存窗口状态（位置、大小等）
                    QRect savedGeometry;
                    bool wasVisible = false;
                    for (QObject* obj : oldRootObjects) {
                        QQuickWindow* quickWindow = qobject_cast<QQuickWindow*>(obj);
                        if (quickWindow) {
                            savedGeometry = quickWindow->geometry();
                            wasVisible = quickWindow->isVisible();
                            quickWindow->setVisible(false);
                        } else {
                            QVariant xProp = obj->property("x");
                            QVariant yProp = obj->property("y");
                            QVariant widthProp = obj->property("width");
                            QVariant heightProp = obj->property("height");
                            QVariant visibleProp = obj->property("visible");
                            if (xProp.isValid() && yProp.isValid() && 
                                widthProp.isValid() && heightProp.isValid()) {
                                savedGeometry = QRect(xProp.toInt(), yProp.toInt(), 
                                                     widthProp.toInt(), heightProp.toInt());
                                wasVisible = visibleProp.toBool();
                                obj->setProperty("visible", false);
                            }
                        }
                    }
                    
                    // 清除组件缓存
                    engine->clearComponentCache();
                    qDebug() << "Component cache cleared";
                    
                    // 清除错误列表
                    qmlErrors.clear();
                    
                    // 删除旧根对象
                    for (QObject* obj : oldRootObjects) {
                        obj->deleteLater();
                    }
                    
                    // 处理事件，确保旧对象被删除
                    QApplication::processEvents();
                    
                    // 重新加载QML文件
                    engine->load(qmlFileUrl);
                    QApplication::processEvents();
                    
                    // 恢复窗口状态并显示
                    QList<QObject*> newRootObjects = engine->rootObjects();
                    for (QObject* obj : newRootObjects) {
                        QQuickWindow* quickWindow = qobject_cast<QQuickWindow*>(obj);
                        if (quickWindow) {
                            if (!savedGeometry.isNull()) {
                                quickWindow->setGeometry(savedGeometry);
                            }
                            if (wasVisible) {
                                quickWindow->show();
                                quickWindow->raise();
                                quickWindow->requestActivate();
                            }
                        } else {
                            if (!savedGeometry.isNull()) {
                                obj->setProperty("x", savedGeometry.x());
                                obj->setProperty("y", savedGeometry.y());
                                obj->setProperty("width", savedGeometry.width());
                                obj->setProperty("height", savedGeometry.height());
                            }
                            if (wasVisible) {
                                QVariant visibleProp = obj->property("visible");
                                if (visibleProp.isValid()) {
                                    obj->setProperty("visible", true);
                                }
                            }
                        }
                    }
                    
                    qDebug() << "Component reloaded, window refreshed with new components";
                }
                
                qDebug() << "========================================";
            };
            
            // 存储最后修改的文件路径
            QString lastChangedFile;
            
            // 连接文件变化信号（使用防抖）
            QObject::connect(fileWatcher, &QFileSystemWatcher::fileChanged, 
                [&lastChangedFile, reloadTimer, fileWatcher, qmlFilePath](const QString &path) {
                    // 只处理QML文件的变化
                    if (path.endsWith(".qml") || path.endsWith(".qmldir")) {
                        qDebug() << "QML file changed:" << path;
                        lastChangedFile = path;
                        
                        // 如果文件被删除后重新创建，需要重新添加监视
                        if (QFile::exists(path) && !fileWatcher->files().contains(path)) {
                            fileWatcher->addPath(path);
                            qDebug() << "Re-added file to watch:" << path;
                        }
                        
                        // 使用防抖定时器，避免频繁触发
                        reloadTimer->stop();
                        reloadTimer->start();
                    }
                });
            
            // 连接防抖定时器超时信号，执行实际的重载
            QObject::connect(reloadTimer, &QTimer::timeout, 
                [reloadQml, &lastChangedFile, qmlFilePath]() {
                    reloadQml(lastChangedFile.isEmpty() ? qmlFilePath : lastChangedFile);
                });
            
            // 连接目录变化信号（新文件添加）
            QObject::connect(fileWatcher, &QFileSystemWatcher::directoryChanged,
                [fileWatcher, config, reloadTimer, reloadQml](const QString &path) {
                    qDebug() << "QML directory changed:" << path;
                    // 重新扫描目录，添加新的QML文件和子目录
                    QDirIterator dirIt(path, QDirIterator::Subdirectories);
                    bool foundNewFiles = false;
                    while (dirIt.hasNext()) {
                        QString itemPath = dirIt.next();
                        QFileInfo fileInfo(itemPath);
                        if (fileInfo.isDir() && !fileInfo.isSymLink()) {
                            // 添加新的子目录
                            if (!fileWatcher->directories().contains(itemPath)) {
                                fileWatcher->addPath(itemPath);
                                qDebug() << "Added new directory to watch:" << itemPath;
                            }
                        } else if (fileInfo.isFile() && (itemPath.endsWith(".qml") || itemPath.endsWith(".qmldir"))) {
                            // 添加新的QML文件
                            if (!fileWatcher->files().contains(itemPath)) {
                                fileWatcher->addPath(itemPath);
                                qDebug() << "Added new QML file to watch:" << itemPath;
                                foundNewFiles = true;
                            }
                        }
                    }
                    // 如果发现新文件，触发重新加载
                    if (foundNewFiles) {
                        reloadTimer->stop();
                        reloadTimer->start();
                    }
                });
        }

        // 加载主 QML 文件
        qDebug() << "Loading QML file:" << qmlFileUrl.toString();
        engine->load(qmlFileUrl);

        // 检查加载结果
        if (engine->rootObjects().isEmpty()) {
            qCritical() << "Failed to load QML file: No root objects created";
            
            // 输出已收集的错误
            if (!qmlErrors.isEmpty()) {
                qCritical() << "QML Errors (" << qmlErrors.size() << "):";
                for (const auto &error : qmlErrors) {
                    qCritical() << "  Error:" << error.toString();
                    qCritical() << "    URL:" << error.url().toString();
                    qCritical() << "    Line:" << error.line() << "Column:" << error.column();
                    qCritical() << "    Description:" << error.description();
                }
            } else {
                qCritical() << "No QML errors were reported, but root objects are empty.";
                qCritical() << "This might indicate:";
                qCritical() << "  1. QML file not found or not accessible";
                qCritical() << "  2. QML file syntax error";
                qCritical() << "  3. Missing QML module imports";
                qCritical() << "  4. Runtime error during object creation";
            }
            
            qCritical() << "========================================";
            qCritical() << "Application will exit with error code -1";
            qCritical() << "Please check the log file for details:" << logFilePath;
            qCritical() << "========================================";
            
            // 确保日志写入
            if (g_logStream) {
                g_logStream->flush();
            }
            
            return -1;
        }

        qDebug() << "QML file loaded successfully";
        qDebug() << "Root objects count:" << engine->rootObjects().size();
        qDebug() << "RoboCute Editor (QML) started successfully";
        if (config.enableHotReload) {
            qDebug() << "Hot reload: QML file changes will be automatically reloaded";
            qDebug() << "Watching QML directory:" << config.qmlDir;
        } else {
            qDebug() << "Hot reload: Disabled (use --hot-reload to enable)";
        }
        qDebug() << "========================================";

        int ret = app.exec();
        
        qDebug() << "Application exiting with code:" << ret;
        
        // 清理日志
        if (g_logStream) {
            g_logStream->flush();
            delete g_logStream;
            g_logStream = nullptr;
        }
        if (g_logFile) {
            g_logFile->close();
            delete g_logFile;
            g_logFile = nullptr;
        }
        
        return ret;
        
    } catch (const std::exception &e) {
        qCritical() << "Exception caught:" << e.what();
        qCritical() << "Please check the log file for details:" << logFilePath;
        
        if (g_logStream) {
            g_logStream->flush();
            delete g_logStream;
            g_logStream = nullptr;
        }
        if (g_logFile) {
            g_logFile->close();
            delete g_logFile;
            g_logFile = nullptr;
        }
        
        return -1;
    } catch (...) {
        qCritical() << "Unknown exception caught!";
        qCritical() << "Please check the log file for details:" << logFilePath;
        
        if (g_logStream) {
            g_logStream->flush();
            delete g_logStream;
            g_logStream = nullptr;
        }
        if (g_logFile) {
            g_logFile->close();
            delete g_logFile;
            g_logFile = nullptr;
        }
        
        return -1;
    }
}

