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
    int syncIntervalMs() const { return sync_interval_ms_; }

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
    std::unique_ptr<EditorScene> scene_;

    // Source tracking
    SceneSourceType source_type_ = SceneSourceType::None;
    QString server_url_;
    QString local_path_;

    // Selection state
    int selected_entity_id_ = -1;

    // Server sync
    ConnectionService *connection_service_ = nullptr;
    HttpClient *http_client_ = nullptr;
    QTimer *sync_timer_ = nullptr;
    int sync_interval_ms_ = 100;// 10 FPS sync rate
    bool sync_in_progress_ = false;

    // Graphics integration
    GraphicsUtils *graphics_utils_ = nullptr;

    // Cached sync data for SERVER mode
    SceneSyncData pending_sync_data_;

    // Internal helpers
    void startSyncTimer();
    void stopSyncTimer();
    void processServerResponse(const QString &jsonData);
    void emitEntityChanges(const SceneSyncData &oldData, const SceneSyncData &newData);
};

}// namespace rbc