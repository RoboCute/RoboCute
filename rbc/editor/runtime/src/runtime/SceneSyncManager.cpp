#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include <QJsonDocument>
#include <QDateTime>

namespace rbc {

SceneSyncManager::SceneSyncManager(HttpClient *httpClient, QObject *parent)
    : QObject(parent),
      httpClient_(httpClient),
      sceneSync_(new SceneSync()),
      syncTimer_(new QTimer(this)),
      heartbeatTimer_(new QTimer(this)),
      isConnected_(false),
      isRunning_(false) {

    // Generate unique editor ID
    editorId_ = QString("qt_editor_%1").arg(QDateTime::currentMSecsSinceEpoch());
    // Setup timers
    syncTimer_->setInterval(1000);     // 1s sync interval
    heartbeatTimer_->setInterval(2000);// 1s heartbeat interval

    connect(syncTimer_, &QTimer::timeout, this, &SceneSyncManager::syncWithServer);
    connect(heartbeatTimer_, &QTimer::timeout, this, &SceneSyncManager::sendHeartbeat);
}

SceneSyncManager::~SceneSyncManager() {
    stop();
    delete sceneSync_;
}

void SceneSyncManager::start(const QString &serverUrl) {
    if (isRunning_) {
        return;
    }
    // Set server URL
    httpClient_->setServerUrl(serverUrl);
    // Register with server
    registerWithServer();
    // Start timers
    syncTimer_->start();
    heartbeatTimer_->start();

    isRunning_ = true;
}

void SceneSyncManager::stop() {
    if (!isRunning_) {
        return;
    }

    syncTimer_->stop();
    heartbeatTimer_->stop();

    isRunning_ = false;
    isConnected_ = false;

    emit connectionStatusChanged(false);
}

void SceneSyncManager::registerWithServer() {
    httpClient_->registerEditor(editorId_, [this](bool success) {
        if (success) {
            isConnected_ = true;
            emit connectionStatusChanged(true);
        } else {
            isConnected_ = false;
            emit connectionStatusChanged(false);
        }
    });
}

void SceneSyncManager::syncWithServer() {
    if (!isRunning_) {
        return;
    }

    // Fetch scene state
    httpClient_->getSceneState([this](const QJsonObject &response, bool success) {
        if (!success) {
            if (isConnected_) {
                isConnected_ = false;
                emit connectionStatusChanged(false);
            }
            return;
        }

        // Mark as connected
        if (!isConnected_) {
            isConnected_ = true;
            emit connectionStatusChanged(true);
        }

        // Parse scene state
        QJsonDocument doc(response);
        QString jsonStr = doc.toJson(QJsonDocument::Compact);
        bool sceneChanged = sceneSync_->parseSceneState(jsonStr.toStdString());

        // Fetch resources if scene changed
        if (sceneChanged) {
            httpClient_->getAllResources([this](const QJsonObject &resResponse, bool resSuccess) {
                if (resSuccess) {
                    QJsonDocument resDoc(resResponse);
                    QString resJsonStr = resDoc.toJson(QJsonDocument::Compact);
                    sceneSync_->parseResources(resJsonStr.toStdString());
                }
            });
        }

        // Also fetch animations
        httpClient_->getAnimations([this](const QJsonObject &animResponse, bool animSuccess) {
            if (animSuccess) {
                QJsonDocument animDoc(animResponse);
                QString animJsonStr = animDoc.toJson(QJsonDocument::Compact);
                sceneSync_->parseAnimations(animJsonStr.toStdString());
            }

            // Emit scene updated signal after all data is synced
            emit sceneUpdated();
        });
    });
}

void SceneSyncManager::sendHeartbeat() {
    if (!isRunning_) {
        return;
    }

    httpClient_->sendHeartbeat(editorId_, [this](bool success) {
        if (!success && isConnected_) {
            isConnected_ = false;
            emit connectionStatusChanged(false);
        }
    });
}

}// namespace rbc
