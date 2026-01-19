#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include "RBCEditorRuntime/services/IService.h"

namespace rbc {

class EditorScene;

namespace world {
struct Entity;
struct TransformComponent;
struct RenderComponent;
}// namespace world

/**
 * Scene source type - determines where scene data comes from
 */
enum class SceneSourceType {
    None,  // No scene loaded
    Server,// Scene synchronized from remote Python server
    Local, // Scene loaded from local .rbcscene file
};

/**
 * Entity info exposed to UI/QML
 */
struct EntityInfo {
    int localId = -1;  // local scene-specific ID
    QString name;      // entity display name
    QString guidString;// GUID as string for serialization
    bool hasRenderComponent = false;
    bool hasTransformComponent = false;
};

/**
 * ISceneService - Interface for scene management in the editor
 * 
 * Provides a unified API for:
 * - Loading scenes from server or local files
 * - Querying and manipulating entities
 * - Notifying UI about scene changes
 * 
 * This service abstracts the underlying scene implementation (EditorScene)
 * and exposes a Qt/QML-friendly interface for the editor UI.
 */
class ISceneService : public IService {
    Q_OBJECT

    // QML-friendly properties
    Q_PROPERTY(bool sceneLoaded READ isSceneLoaded NOTIFY sceneLoadedChanged)
    Q_PROPERTY(int entityCount READ entityCount NOTIFY entityCountChanged)
    Q_PROPERTY(QString sceneSourceName READ sceneSourceName NOTIFY sceneSourceChanged)
    Q_PROPERTY(int selectedEntityId READ selectedEntityId WRITE setSelectedEntityId NOTIFY selectedEntityChanged)

public:
    explicit ISceneService(QObject *parent = nullptr) : IService(parent) {}
    virtual ~ISceneService() = default;

    QString serviceId() const override { return "com.robocute.scene_service"; }

    // ========== Scene Loading ==========

    /**
     * Load scene from remote server
     * @param serverUrl The server URL (e.g., "http://127.0.0.1:5555")
     * @return true if connection initiated successfully
     */
    virtual bool loadFromServer(const QString &serverUrl) = 0;

    /**
     * Load scene from local .rbcscene file or project directory
     * @param scenePath Path to the scene file or project folder
     * @return true if loading initiated successfully
     */
    virtual bool loadFromLocal(const QString &scenePath) = 0;

    /**
     * Unload the current scene and release resources
     */
    virtual void unloadScene() = 0;

    /**
     * Get current scene source type
     */
    virtual SceneSourceType sceneSource() const = 0;

    /**
     * Get scene source as human-readable string
     */
    virtual QString sceneSourceName() const = 0;

    // ========== Scene State ==========

    /**
     * Check if a scene is currently loaded and ready
     */
    virtual bool isSceneLoaded() const = 0;

    /**
     * Check if the scene is fully ready for rendering
     */
    virtual bool isSceneReady() const = 0;

    /**
     * Get the number of entities in the scene
     */
    virtual int entityCount() const = 0;

    // ========== Entity Access ==========

    /**
     * Get all entity IDs in the scene
     */
    virtual QList<int> getAllEntityIds() const = 0;

    /**
     * Get entity info by local ID
     */
    virtual EntityInfo getEntityInfo(int localId) const = 0;

    /**
     * Get all entities info (for UI listing)
     */
    virtual QList<EntityInfo> getAllEntities() const = 0;

    /**
     * Get entity by local ID (returns nullptr if not found)
     * For internal C++ use only - provides direct access to world::Entity
     */
    virtual world::Entity *getEntity(int localId) const = 0;

    /**
     * Get entity ID from TLAS instance ID (for picking)
     */
    virtual int getEntityIdFromInstanceId(uint32_t instanceId) const = 0;

    // ========== Entity Selection ==========

    /**
     * Get currently selected entity ID (-1 if none)
     */
    virtual int selectedEntityId() const = 0;

    /**
     * Set selected entity by local ID
     */
    virtual void setSelectedEntityId(int localId) = 0;

    /**
     * Clear entity selection
     */
    virtual void clearSelection() = 0;

    // ========== Scene Updates ==========

    /**
     * Called each frame to update scene state
     * - For SERVER mode: processes network updates
     * - For LOCAL mode: processes async resource loading
     */
    virtual void tick() = 0;

    /**
     * Synchronize scene with server (SERVER mode only)
     * Fetches latest state from Python server
     */
    virtual void syncFromServer() = 0;

    /**
     * Save scene modifications (LOCAL mode only)
     */
    virtual bool saveScene() = 0;

    // ========== Render Integration ==========

    /**
     * Get the underlying EditorScene for render integration
     * Returns nullptr if no scene is loaded
     */
    virtual EditorScene *editorScene() const = 0;

signals:
    // Scene lifecycle
    void sceneLoadedChanged();
    void sceneSourceChanged();
    void sceneReadyChanged();

    // Entity changes
    void entityCountChanged();
    void entityAdded(int localId);
    void entityRemoved(int localId);
    void entityTransformChanged(int localId);

    // Selection
    void selectedEntityChanged();

    // Errors
    void loadError(const QString &message);
    void syncError(const QString &message);
};

}// namespace rbc