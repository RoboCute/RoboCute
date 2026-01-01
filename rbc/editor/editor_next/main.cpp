#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QLoggingCategory>
#include <QStandardPaths>

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
    // 启用高 DPI 支持
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setApplicationName("RoboCute Editor");
    app.setApplicationVersion("0.3.0-next");

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
        // 注册 QML 类型
        qDebug() << "Registering QML types...";
        rbc::registerQmlTypes();
        qDebug() << "QML types registered successfully";

        // 创建 QML 引擎
        QQmlApplicationEngine engine;
        
        // 添加 QML 导入路径（用于查找 qmldir 和组件）
        engine.addImportPath("qrc:/qml");
        qDebug() << "QML import paths:" << engine.importPathList();

        // 存储错误列表（用于在加载后检查）
        QList<QQmlError> qmlErrors;
        
        // 连接 QML 错误和警告信号
        QObject::connect(&engine, &QQmlApplicationEngine::warnings, 
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
        engine.rootContext()->setContextProperty("globalEditorService", editorService);
        engine.rootContext()->setContextProperty("globalConnectionService", connectionService);
        qDebug() << "Context properties registered";

        // 设置 QML 文件路径（支持热重载）
        // 优先使用资源文件，开发时可以使用文件系统路径
        const QUrl qmlFile(QStringLiteral("qrc:/qml/MainWindow.qml"));
        
        // 开发模式：如果存在文件系统路径，使用文件系统（支持热重载）
        // 生产模式：使用资源文件
        QUrl actualQmlFile = qmlFile;
        
        // 检查是否存在文件系统路径（开发时使用）
        QString fsPath = QDir::currentPath() + "/rbc/editor/editor_next/qml/MainWindow.qml";
        if (QFile::exists(fsPath)) {
            actualQmlFile = QUrl::fromLocalFile(fsPath);
            qDebug() << "Using file system QML (hot reload enabled):" << actualQmlFile;
        } else {
            qDebug() << "Using resource QML:" << actualQmlFile;
            // 验证资源文件是否存在
            if (!QFile::exists(":/qml/MainWindow.qml")) {
                qCritical() << "QML resource file not found: qrc:/qml/MainWindow.qml";
                qCritical() << "Please check rbc_editor.qrc file";
            }
        }

        // 加载主 QML 文件
        qDebug() << "Loading QML file:" << actualQmlFile.toString();
        engine.load(actualQmlFile);

        // 检查加载结果
        if (engine.rootObjects().isEmpty()) {
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
        qDebug() << "Root objects count:" << engine.rootObjects().size();
        qDebug() << "RoboCute Editor (QML) started successfully";
        qDebug() << "Hot reload: Press Ctrl+R in Qt Creator to reload QML";
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

