#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include "RBCEditorRuntime/runtime/HttpClient.h"
#include "RBCEditorRuntime/runtime/SceneSync.h"

namespace rbc {

/**
 * Scene Sync Manager
 * 
 * Manages the scene synchronization lifecycle:
 * - Periodic sync with Python server (100ms intervals)
 * - Heartbeat to keep connection alive (1s intervals)
 * - Coordinates HttpClient, SceneSync, and EditorScene
 * - Emits signals for UI updates
 */
class SceneSyncManager : public QObject {
    Q_OBJECT

public:
    explicit SceneSyncManager(HttpClient *httpClient, QObject *parent = nullptr);
    ~SceneSyncManager() override;

    // Start/stop sync
    void start(const QString &serverUrl);
    void stop();

    // Get sync state
    const SceneSync *sceneSync() const { return sceneSync_; }
    bool isConnected() const { return isConnected_; }
    QString editorId() const { return editorId_; }

signals:
    // Emitted when scene state is updated from server
    void sceneUpdated();

    // Emitted when connection status changes
    void connectionStatusChanged(bool connected);

private slots:
    void syncWithServer();
    void sendHeartbeat();

private:
    void registerWithServer();

    HttpClient *httpClient_;
    SceneSync *sceneSync_;

    QTimer *syncTimer_;
    QTimer *heartbeatTimer_;

    QString editorId_;
    bool isConnected_;
    bool isRunning_;
};

}// namespace rbc
