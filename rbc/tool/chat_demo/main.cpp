#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "ChatClient.h"
#include "ChatDemoWidget.h"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

QString findProjectRoot() {
    // The launcher is expected to run from either the build output directory or
    // the repository root. Walk up until we find the server script.
    QDir dir(QCoreApplication::applicationDirPath());
    for (int i = 0; i < 6; ++i) {
        if (QFile::exists(dir.absoluteFilePath(QStringLiteral("tools/llama_chat_demo/chat_server.py")))) {
            return dir.absolutePath();
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QDir::currentPath();
}

struct ServerUrlResult {
    QString url;
    bool auto_start = false;
};

ServerUrlResult parseServerUrl(int argc, char *argv[]) {
    ServerUrlResult result;
    result.url = QString::fromLocal8Bit(qgetenv("RBC_CHAT_SERVER_URL"));

    QStringList args;
    for (int i = 1; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    for (int i = 0; i < args.size(); ++i) {
        if ((args[i] == QStringLiteral("--server-url") || args[i] == QStringLiteral("-s")) && i + 1 < args.size()) {
            result.url = args[i + 1];
            ++i;
        }
    }

    if (result.url.isEmpty() || !result.url.startsWith(QStringLiteral("http"))) {
        result.url = QStringLiteral("http://127.0.0.1:8123");
        result.auto_start = true;
    }
    return result;
}

bool waitForServerHealth(const QString &url, int timeoutMs = 10000) {
    QElapsedTimer timer;
    timer.start();
    QNetworkAccessManager manager;
    while (timer.elapsed() < timeoutMs) {
        QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(url + QStringLiteral("/health"))));
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(500, &loop, &QEventLoop::quit);
        loop.exec();
        bool ok = (reply->error() == QNetworkReply::NoError);
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (ok && status == 200) {
            return true;
        }
        if (timer.elapsed() >= timeoutMs) {
            break;
        }
        QThread::msleep(250);
    }
    return false;
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    ServerUrlResult server = parseServerUrl(argc, argv);
    qDebug() << "Chat demo launcher; server URL:" << server.url;

    QProcess pythonServer;
    bool startedServer = false;
    if (server.auto_start) {
        QString projectRoot = findProjectRoot();
        QString serverScript = QDir(projectRoot).absoluteFilePath(QStringLiteral("tools/llama_chat_demo/chat_server.py"));
        if (!QFile::exists(serverScript)) {
            qWarning() << "Cannot find chat server script at" << serverScript;
            qWarning() << "Please start the server manually and pass --server-url";
        } else {
            qDebug() << "Auto-starting Python chat server:" << serverScript;
            pythonServer.setWorkingDirectory(projectRoot);
            pythonServer.setProgram(QStringLiteral("uv"));
            pythonServer.setArguments({
                QStringLiteral("run"),
                QStringLiteral("--python"),
                QStringLiteral("3.14.3"),
                QStringLiteral("--no-project"),
                QStringLiteral("--with"),
                QStringLiteral("fastapi"),
                QStringLiteral("--with"),
                QStringLiteral("uvicorn"),
                QStringLiteral("--with"),
                QStringLiteral("httpx"),
                QStringLiteral("--with"),
                QStringLiteral("pydantic"),
                QStringLiteral("python"),
                serverScript,
                QStringLiteral("--port"),
                QStringLiteral("8123"),
            });
            pythonServer.start();
            if (!pythonServer.waitForStarted(30000)) {
                qWarning() << "Failed to start Python chat server:" << pythonServer.errorString();
            } else {
                startedServer = true;
                if (!waitForServerHealth(server.url)) {
                    qWarning() << "Python chat server did not become healthy in time";
                } else {
                    qDebug() << "Python chat server is healthy";
                }
            }
        }
    }

    QMainWindow mainWindow;
    mainWindow.setWindowTitle(QStringLiteral("RoboCute Chat Demo"));
    mainWindow.resize(1280, 720);

    auto *client = new ChatClient(&mainWindow);
    client->setServerUrl(QUrl(server.url));

    auto *widget = new ChatDemoWidget(client, &mainWindow);
    widget->setServerUrl(server.url);
    mainWindow.setCentralWidget(widget);

    mainWindow.show();
    int result = app.exec();

    if (startedServer && pythonServer.state() != QProcess::NotRunning) {
        qDebug() << "Terminating Python chat server";
        pythonServer.terminate();
        if (!pythonServer.waitForFinished(5000)) {
            pythonServer.kill();
            pythonServer.waitForFinished(3000);
        }
    }

    return result;
}
