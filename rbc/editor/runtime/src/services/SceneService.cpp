#include "RBCEditorRuntime/services/SceneService.h"

namespace rbc {

SceneService::SceneService(QObject *parent) {}
SceneService::~SceneService() {}
// == scene loading
bool SceneService::loadFromServer(const QString &serverUrl) {
    return false;
}
bool SceneService::loadFromLocal(const QString &scenePath) {
    return false;
}

void SceneService::unloadScene() {}

SceneSourceType SceneService::sceneSource() const {
}

QString SceneService::sceneSourceName() const {
}

// == scene state
bool SceneService::isSceneLoaded() const {
    return false;
}

bool SceneService::isSceneReady() const {
    return false;
}

int SceneService::entityCount() const {
    return 0;
}

QList<int> SceneService::getAllEntityIds() const {
    return {};
}

EntityInfo SceneService::getEntityInfo(int localId) const {
    return {};
}

QList<EntityInfo> SceneService::getAllEntities() const {
    return {};
}

world::Entity *SceneService::getEntity(int localId) const {
    return nullptr;
}
int SceneService::getEntityIdFromInstanceId(uint32_t instanceId) const {
    return 0;
}

// == Entity Selection
int SceneService::selectedEntityId() const {
    return 0;
}

void SceneService::setSelectedEntityId(int localId) {
}

void SceneService::clearSelection() {
}

// == Scene Update
void SceneService::tick() {
}

void SceneService::syncFromServer() {
}

bool SceneService::saveScene() {
    return false;
}

EditorScene *SceneService::editorScene() const {
    return nullptr;
}

void SceneService::setConnectionService(ConnectionService *connectionService) {
}

void SceneService::setGraphicsUtils(GraphicsUtils *graphicsUtils) {
}

void SceneService::setSyncIntervalMs(int ms) {
    sync_interval_ms_ = ms;
}

void SceneService::startSyncTimer() {}
void SceneService::stopSyncTimer() {
}
void SceneService::processServerResponse(const QString &jsonData) {
}

void SceneService::emitEntityChanges(const SceneSyncData &oldData, const SceneSyncData &newData) {
}

void SceneService::onSyncTimerTimeout() {}
void SceneService::onSceneDataReceived(const QString &jsonData) {}
void SceneService::onSceneDataError(const QString &error) {}

}// namespace rbc