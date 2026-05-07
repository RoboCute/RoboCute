#pragma once

#include <rbc_config.h>
#include <QObject>
#include <QTimer>
#include <memory>
#include "RBCEditorRuntime/services/ISceneService.h"
#include "RBCEditorRuntime/infra/editor/EditorScene.h"

namespace rbc {

class ConnectionService;
class HttpClient;

/**
 * SceneService - Concrete implementation of ISceneService
 * 
 * Provides scene management for the editor by delegating to EditorScene.
 * Handles:
 * - Scene loading from server or local files
 * - Periodic sync with server (when in SERVER mode)
 * - Qt signal emission for UI updates
 * - Thread-safe access to scene data
 */
class RBC_EDITOR_RUNTIME_API SceneService : public ISceneService {
    Q_OBJECT

public:
    explicit SceneService(QObject *parent = nullptr);
    ~SceneService() override;

    // ========== Scene Loading (ISceneService) ==========
    bool loadFromServer(const QString &serverUrl) override;
    bool loadFromLocal(const QString &scenePath) override;
    void unloadScene() override;
    SceneSourceType sceneSource() const override;
    QString sceneSourceName() const override;

    // ========== Scene State (ISceneService) ==========
    bool isSceneLoaded() const override;
    bool isSceneReady() const override;
    int entityCount() const override;

    // ========== Entity Access (ISceneService) ==========
    QList<int> getAllEntityIds() const override;
    EntityInfo getEntityInfo(int localId) const override;
    QList<EntityInfo> getAllEntities() const override;
    world::Entity *getEntity(int localId) const override;
    int getEntityIdFromInstanceId(uint32_t instanceId) const override;

    // ========== Entity Selection (ISceneService) ==========
    int selectedEntityId() const override;
    void setSelectedEntityId(int localId) override;
    void clearSelection() override;

    // ========== Scene Updates (ISceneService) ==========
    void tick() override;
    void syncFromServer() override;
    bool saveScene() override;

    // ========== Render Integration (ISceneService) ==========
    EditorScene *editorScene() const override;

    // ========== Service-specific Methods ==========

    /**
     * Set the connection service for server mode
     * Required before calling loadFromServer
     */
    void setConnectionService(ConnectionService *connectionService);

    /**
     * Set the graphics utils for resource loading
     */
    void setGraphicsUtils(GraphicsUtils *graphicsUtils);

    /**
     * Get sync interval in milliseconds (SERVER mode)
     */
    int syncIntervalMs() const { return _sync_interval_ms; }

    /**
     * Set sync interval in milliseconds (SERVER mode)
     */
    void setSyncIntervalMs(int ms);

private slots:
    void onSyncTimerTimeout();
    void onSceneDataReceived(const QString &jsonData);
    void onSceneDataError(const QString &error);

private:
    // Scene instance
    std::unique_ptr<EditorScene> _scene;

    // Source tracking
    SceneSourceType _source_type = SceneSourceType::None;
    QString _server_url;
    QString _local_path;

    // Selection state
    int _selected_entity_id = -1;

    // Server sync
    ConnectionService *_connection_service = nullptr;
    HttpClient *_http_client = nullptr;
    QTimer *_sync_timer = nullptr;
    int _sync_interval_ms = 100;// 10 FPS sync rate
    bool _sync_in_progress = false;

    // Graphics integration
    GraphicsUtils *_graphics_utils = nullptr;

    // Cached sync data for SERVER mode
    SceneSyncData _pending_sync_data;

    // Internal helpers
    void startSyncTimer();
    void stopSyncTimer();
    void processServerResponse(const QString &jsonData);
    void emitEntityChanges(const SceneSyncData &oldData, const SceneSyncData &newData);
};

}// namespace rbc