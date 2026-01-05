#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"

namespace rbc {

PluginManager &PluginManager::instance() {
    static PluginManager mng;
    return mng;
}

bool PluginManager::loadPlugin(const QString &pluginPath) {
    return true;
}

bool PluginManager::loadPlugin(IEditorPlugin *plugin) {
    return true;
}

bool PluginManager::unloadPlugin(const QString &pluginId) {
    return true;
}

bool PluginManager::reloadPlugin(const QString &pluginId) {
    return true;
}

void PluginManager::unloadAllPlugins() {
}

IEditorPlugin *PluginManager::getPlugin(const QString &id) const {
    return nullptr;
}

QList<IEditorPlugin *> PluginManager::getLoadedPlugins() const {
    return {};
}

QList<IEditorPlugin *> PluginManager::getPluginsByCategory(const QString &category) const {
    return {};
}

void PluginManager::enableHotReload(bool enable) {
}

bool PluginManager::isHostReloadEnabled() const {
}

void PluginManager::watchPluginDirectory(const QString &path) {
}

void PluginManager::registerService(const QString &name, QObject *service) {
}

QObject *PluginManager::getService(const QString &name) const {
    return nullptr;
}

void PluginManager::setQmlEngine(QQmlEngine *engine) {}

QQmlEngine *PluginManager::qmlEngine() {}

PluginManager::PluginManager() {}

void PluginManager::resolvePluginDependencies() {}

void PluginManager::initializePlugin(IEditorPlugin *plugin) {}

}// namespace rbc