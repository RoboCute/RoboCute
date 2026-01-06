#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

// services
#include "RBCEditorRuntime/services/ConnectionService.h"

namespace rbc {

EditorPluginManager &EditorPluginManager::instance() {
    static EditorPluginManager mng;
    return mng;
}

bool EditorPluginManager::loadPlugin(const QString &pluginPath) {
    return true;
}

bool EditorPluginManager::loadPlugin(IEditorPlugin *plugin) {
    return true;
}

bool EditorPluginManager::unloadPlugin(const QString &pluginId) {
    return true;
}

bool EditorPluginManager::reloadPlugin(const QString &pluginId) {
    return true;
}

void EditorPluginManager::unloadAllPlugins() {
}

IEditorPlugin *EditorPluginManager::getPlugin(const QString &id) const {
    return nullptr;
}

QList<IEditorPlugin *> EditorPluginManager::getLoadedPlugins() const {
    return {};
}

QList<IEditorPlugin *> EditorPluginManager::getPluginsByCategory(const QString &category) const {
    return {};
}

void EditorPluginManager::enableHotReload(bool enable) {
}

bool EditorPluginManager::isHostReloadEnabled() const {
}

void EditorPluginManager::watchPluginDirectory(const QString &path) {
}

void EditorPluginManager::registerService(const QString &name, QObject *service) {
}

QObject *EditorPluginManager::getService(const QString &name) const {
    return nullptr;
}

void EditorPluginManager::setQmlEngine(QQmlEngine *engine) {}

QQmlEngine *EditorPluginManager::qmlEngine() {}

EditorPluginManager::EditorPluginManager() {}

void EditorPluginManager::resolvePluginDependencies() {}

void EditorPluginManager::initializePlugin(IEditorPlugin *plugin) {}

}// namespace rbc