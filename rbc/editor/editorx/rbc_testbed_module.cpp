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

    // Get QML file path or watch directory path
    QString qmlPath;
    bool enable_hot_reload = parser.isSet(enableHotReloadOption);
    qDebug() << "Hot Reload 【" << (enable_hot_reload ? "enabled" : "disabled") << "】\n";
    if (parser.isSet(qmlPathOption)) {
        qmlPath = parser.value(qmlPathOption);
    } else {
        // Default path is `qrc://`
        qmlPath = "qrc://";
    }
    QString qmlFilePath;// main.qml path
    QString watchDir;
    if (enable_hot_reload) {
        // if enable hot reload, qmlPath should be provided
        QFileInfo pathInfo(qmlPath);
        if (!pathInfo.exists()) {
            qWarning() << "QML path not found:" << qmlPath;
            qWarning() << "Usage: --enable_hot_reload --qml_path <path_to_qml_file_or_directory>";
            return -1;
        }
        qmlPath = pathInfo.absoluteFilePath();
        watchDir = qmlPath;
        QDir dir(watchDir);
        qmlFilePath = dir.absoluteFilePath("MainWindow.qml");
        qputenv("QML_DISABLE_DISK_CACHE", "1");
        qputenv("QT_QUICK_COMPILER_DISABLE_CACHE", "1");
    } else {
        qmlFilePath = "qrc://qml/MainWindow.qml";
    }

    qDebug() << "Loading QML from:" << qmlPath;
    QQmlApplicationEngine engine;
    // Connect warning signal to view QML errors
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const QQmlError &error : warnings) {
            qWarning() << "QML Error:" << error.toString();
        }
    });
    if (enable_hot_reload) {
        engine.addImportPath(qmlPath);

        QFileInfo mainQmlInfo(qmlPath);
        QUrl baseUrl = QUrl::fromLocalFile(mainQmlInfo.absolutePath() + "/");
        engine.setBaseUrl(baseUrl);
    }
    QUrl qmlUrl = QUrl::fromLocalFile(qmlFilePath);
    engine.load(qmlUrl);
    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Failed to load QML file";
        return -1;
    }
    // If hot reload enabled, set up directory watcher
    if (enable_hot_reload) {
        QFileSystemWatcher *watcher = new QFileSystemWatcher(&app);
        // Recursively find all QML files in directory and add to watch list
        QDir dir(watchDir);
        QStringList qmlFiles;
        QDirIterator it(watchDir, QStringList() << "*.qml", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next();
            QFileInfo fileInfo(filePath);
            if (fileInfo.isFile()) {
                watcher->addPath(filePath);
                qmlFiles << filePath;
            }
        }
        watcher->addPath(watchDir);
        qDebug() << "Watching" << qmlFiles.size() << "QML files in directory:" << watchDir;
        for (const QString &file : qmlFiles) {
            qDebug() << "  -" << file;
        }
        // Use pointer to capture engine, ensuring lambda can access it safely
        QQmlApplicationEngine *enginePtr = &engine;
        QString mainQmlPath = qmlFilePath;
        QString watchDirectory = watchDir;
        QTimer *reloadTimer = new QTimer(&app);
        reloadTimer->setSingleShot(true);
        reloadTimer->setInterval(200);// 200ms debounce delay
        auto doReload = [enginePtr, mainQmlPath, watchDirectory]() {
            qDebug() << "QML file changed, reloading...";
            // Get current app instance
            QGuiApplication *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
            bool originalQuit = app->quitOnLastWindowClosed();
            app->setQuitOnLastWindowClosed(false);
            QObjectList oldRootObjects = enginePtr->rootObjects();
            QWindow *oldWindow = nullptr;
            QRect oldGeometry;
            bool hasOldWindow = false;
            for (QObject *obj : oldRootObjects) {
                if (QWindow *window = qobject_cast<QWindow *>(obj)) {
                    oldWindow = window;
                    oldGeometry = window->geometry();
                    hasOldWindow = true;
                    break;
                }
            }
            for (QObject *obj : oldRootObjects) {
                delete obj;// Delete immediately to release references
            }
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

            enginePtr->clearComponentCache();
            enginePtr->trimComponentCache();
            QFileInfo mainQmlInfo(mainQmlPath);
            QUrl baseUrl = QUrl::fromLocalFile(mainQmlInfo.absolutePath() + "/");
            enginePtr->setBaseUrl(baseUrl);
            QUrl qmlUrl = QUrl::fromLocalFile(mainQmlPath);
            enginePtr->load(qmlUrl);

            QCoreApplication::processEvents();
            QObjectList newRootObjects = enginePtr->rootObjects();
            QWindow *newWindow = nullptr;
            for (QObject *obj : newRootObjects) {
                if (QWindow *window = qobject_cast<QWindow *>(obj)) {
                    newWindow = window;
                    break;
                }
            }
            if (newWindow) {
                qDebug() << "QML file reloaded successfully";
                // Restore window position and size
                if (hasOldWindow) {
                    newWindow->setGeometry(oldGeometry);
                }
                if (!newWindow->isVisible()) {
                    newWindow->show();
                }
            } else {
                qWarning() << "Failed to reload QML file - check for syntax errors!";
            }
            app->setQuitOnLastWindowClosed(originalQuit);
        };

        // Connect timer to reload function
        QObject::connect(reloadTimer, &QTimer::timeout, doReload);
        auto reloadQml = [reloadTimer]() {
            // If timer is running, reset it (debounce)
            if (reloadTimer->isActive()) {
                reloadTimer->stop();
            }
            // Start debounce timer
            reloadTimer->start();
        };
        // Watch file changes
        QObject::connect(
            watcher, &QFileSystemWatcher::fileChanged,
            [watcher, watchDirectory, reloadQml](const QString &path) {
                // Only handle QML file changes
                if (path.endsWith(".qml", Qt::CaseInsensitive)) {
                    qDebug() << "QML file changed:" << path;

                    // Re-add watch (file change might temporarily remove watch)
                    QTimer::singleShot(100, [watcher, path]() {
                        if (!watcher->files().contains(path)) {
                            watcher->addPath(path);
                        }
                    });

                    // Trigger reload (with debounce)
                    reloadQml();
                }
            });

        // Watch directory changes (new files)
        QObject::connect(
            watcher, &QFileSystemWatcher::directoryChanged,
            [watcher, watchDirectory, reloadQml](const QString &path) {
                qDebug() << "Directory changed:" << path;

                // Re-add directory watch
                QTimer::singleShot(100, [watcher, watchDirectory]() {
                    if (!watcher->directories().contains(watchDirectory)) {
                        watcher->addPath(watchDirectory);
                    }
                });

                // Rescan directory, add new QML files to watch list
                QDir dir(watchDirectory);
                QDirIterator it(watchDirectory, QStringList() << "*.qml", QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    QString filePath = it.next();
                    QFileInfo fileInfo(filePath);
                    if (fileInfo.isFile() && !watcher->files().contains(filePath)) {
                        watcher->addPath(filePath);
                        qDebug() << "Added new QML file to watch:" << filePath;
                    }
                }

                // Trigger reload
                reloadQml();
            });
        qDebug() << "Directory watcher initialized for:" << watchDir;
    }
    int res = app.exec();
    return res;
}