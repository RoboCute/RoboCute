#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QDebug>

#include "RBCEditorRuntime/qml/QmlTypes.h"
#include "RBCEditorRuntime/qml/ConnectionService.h"
#include "RBCEditorRuntime/qml/EditorService.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

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

    // 注册 QML 类型
    rbc::registerQmlTypes();

    // 创建 QML 引擎
    QQmlApplicationEngine engine;

    // 创建全局服务实例（可选，也可以通过 QML 创建）
    auto *editorService = new rbc::EditorService(&app);
    auto *connectionService = new rbc::ConnectionService(&app);

    // 将服务注册为上下文属性（全局访问）
    engine.rootContext()->setContextProperty("globalEditorService", editorService);
    engine.rootContext()->setContextProperty("globalConnectionService", connectionService);

    // 设置 QML 文件路径（支持热重载）
    // 优先使用资源文件，开发时可以使用文件系统路径
    const QUrl qmlFile(QStringLiteral("qrc:/qml/MainWindow.qml"));
    
    // 开发模式：如果存在文件系统路径，使用文件系统（支持热重载）
    // 生产模式：使用资源文件
    QUrl actualQmlFile = qmlFile;
    
    // 检查是否存在文件系统路径（开发时使用）
    QString fsPath = QDir::currentPath() + "/rbc/editor/runtime_next/qml/MainWindow.qml";
    if (QFile::exists(fsPath)) {
        actualQmlFile = QUrl::fromLocalFile(fsPath);
        qDebug() << "Using file system QML (hot reload enabled):" << actualQmlFile;
    } else {
        qDebug() << "Using resource QML:" << actualQmlFile;
    }

    // 连接对象创建失败信号
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    // 加载主 QML 文件
    engine.load(actualQmlFile);

    if (engine.rootObjects().isEmpty()) {
        qCritical() << "Failed to load QML file";
        return -1;
    }

    qDebug() << "RoboCute Editor (QML) started successfully";
    qDebug() << "Hot reload: Press Ctrl+R in Qt Creator to reload QML";

    return app.exec();
}

