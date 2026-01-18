#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQmlContext>
#include <QFileSystemWatcher>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>
#include <QCommandLineParser>
#include <QTimer>
#include <QWindow>
#include <QCoreApplication>
#include <QStringList>
#include <QtGlobal>
#include <rbc_config.h>

LUISA_EXPORT_API int dll_main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
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
}