#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QDebug>
#include <QDir>

namespace rbc {

EditorPluginManager &EditorPluginManager::instance() {
    static EditorPluginManager mng;
    return mng;
}

EditorPluginManager::EditorPluginManager()
    : QObject(nullptr), _qml_engine(nullptr), _hot_reload_watcher(nullptr), _hot_reload_enabled(false) {
}

EditorPluginManager::~EditorPluginManager() {
    // If clearServices() was not called, do final cleanup here
    // But if services have been deleted by their parent, accessing them will crash
    if (!_services.isEmpty()) {
        qWarning() << "EditorPluginManager::~EditorPluginManager: clearServices() was not called!";
        qWarning() << "This may cause crashes if services have already been destroyed by their parent.";
        // Do not try to access services, just clear references
        _services.clear();
    }

    // Unload all plugins
    unloadAllPlugins();

    // Clear factories (after plugins are unloaded)
    _factories.clear();
}

// === Factory Registration ===

void EditorPluginManager::registerFactory(std::unique_ptr<IPluginFactory> factory) {
    if (!factory) {
        qWarning() << "EditorPluginManager::registerFactory: factory is null";
        return;
    }

    QString id = factory->pluginId();
    if (_factories.find(id) != _factories.end()) {
        qWarning() << "EditorPluginManager::registerFactory: Factory for" << id << "already registered";
        return;
    }

    qDebug() << "EditorPluginManager::registerFactory: Registered factory for" << id
             << "(" << factory->pluginName() << ")";
    _factories[id] = std::move(factory);
}

// === Plugin LifeCycle ===

bool EditorPluginManager::loadPlugin(const QString &pluginId) {
    if (_plugins.find(pluginId) != _plugins.end()) {
        qWarning() << "EditorPluginManager::loadPlugin: Plugin" << pluginId << "already loaded";
        return false;
    }

    auto factoryIt = _factories.find(pluginId);
    if (factoryIt == _factories.end()) {
        qWarning() << "EditorPluginManager::loadPlugin: No factory registered for" << pluginId;
        return false;
    }

    // Create plugin through factory
    auto plugin = factoryIt->second->create();
    if (!plugin) {
        qWarning() << "EditorPluginManager::loadPlugin: Factory failed to create plugin" << pluginId;
        return false;
    }

    return loadPluginInternal(std::move(plugin), pluginId);
}

bool EditorPluginManager::loadPluginFromDLL(const QString &pluginPath) {
    auto &inst = rbc::PluginManager::instance();
    auto module = inst.load_module(pluginPath.toStdString().c_str());

    // New design: dynamic library exports createPluginFactory function
    IPluginFactory *factoryPtr = module->invoke<IPluginFactory *()>("createPluginFactory");
    if (!factoryPtr) {
        qWarning() << "EditorPluginManager::loadPluginFromDLL: createPluginFactory returned null from" << pluginPath;
        return false;
    }

    // Take over factory ownership
    std::unique_ptr<IPluginFactory> factory(factoryPtr);
    QString pluginId = factory->pluginId();

    // Create plugin through factory
    auto plugin = factory->create();
    if (!plugin) {
        qWarning() << "EditorPluginManager::loadPluginFromDLL: Factory failed to create plugin from" << pluginPath;
        return false;
    }

    plugin->set_plugin_path(pluginPath);

    if (!loadPluginInternal(std::move(plugin), pluginId)) {
        return false;
    }

    // Keep DLL module loaded to prevent plugin code unloading
    _modules[pluginId] = std::move(module);

    // Save factory to support reloading
    _factories[pluginId] = std::move(factory);

    return true;
}

bool EditorPluginManager::loadPluginInternal(std::unique_ptr<IEditorPlugin> plugin, const QString &pluginId) {
    if (!plugin) {
        qWarning() << "EditorPluginManager::loadPluginInternal: plugin is null";
        return false;
    }

    // Create PluginContext
    PluginContext *context = new PluginContext(&instance(), this);

    // Load plugin
    if (!plugin->load(context)) {
        qWarning() << "EditorPluginManager::loadPluginInternal: Failed to load plugin" << pluginId;
        delete context;
        return false;
    }

    // Register ViewModels
    if (_qml_engine) {
        plugin->register_view_models(_qml_engine);
    }

    // Initialize plugin
    initializePlugin(plugin.get());

    QString pluginName = plugin->name();

    // Manage plugin lifecycle with unique_ptr
    _plugins[pluginId] = std::move(plugin);

    qDebug() << "EditorPluginManager::loadPluginInternal: Plugin" << pluginId
             << "(" << pluginName << ") loaded successfully";
    emit pluginLoaded(pluginId);

    return true;
}

bool EditorPluginManager::unloadPlugin(const QString &pluginId) {
    auto it = _plugins.find(pluginId);
    if (it == _plugins.end()) {
        qWarning() << "EditorPluginManager::unloadPlugin: Plugin" << pluginId << "not found";
        return false;
    }

    IEditorPlugin *plugin = it->second.get();

    // Check dependencies
    for (auto otherIt = _plugins.begin(); otherIt != _plugins.end(); ++otherIt) {
        IEditorPlugin *otherPlugin = otherIt->second.get();
        if (otherPlugin != plugin) {
            QStringList deps = otherPlugin->dependencies();
            if (deps.contains(pluginId)) {
                qWarning() << "EditorPluginManager::unloadPlugin: Cannot unload" << pluginId
                           << "because" << otherPlugin->id() << "depends on it";
                return false;
            }
        }
    }

    // Save plugin_path for subsequent DLL unloading
    QString pluginPath = plugin->plugin_path();
    auto moduleIt = _modules.find(pluginId);
    bool hasDynamicModule = (moduleIt != _modules.end());

    // If dynamic library plugin, keep module loaded for virtual function calls
    luisa::shared_ptr<luisa::DynamicModule> moduleRef;
    if (hasDynamicModule) {
        moduleRef = moduleIt->second;
    }

    // Call unload (DLL is still loaded at this point)
    if (!plugin->unload()) {
        qWarning() << "EditorPluginManager::unloadPlugin: Failed to unload plugin" << pluginId;
        return false;
    }

    // Remove from map, unique_ptr auto-deletes plugin object
    // This must be done before DLL unloading
    _plugins.erase(it);

    // Unload DLL module (plugin object already deleted)
    if (hasDynamicModule) {
        _modules.erase(pluginId);

        auto &inst = rbc::PluginManager::instance();
        if (!pluginPath.isEmpty()) {
            inst.unload_module(pluginPath.toStdString().c_str());
        }

        // Also remove corresponding factory (because factory code is also in DLL)
        _factories.erase(pluginId);
    }

    qDebug() << "EditorPluginManager::unloadPlugin: Plugin" << pluginId << "unloaded";
    emit pluginUnloaded(pluginId);

    return true;
}

bool EditorPluginManager::reloadPlugin(const QString &pluginId) {
    auto it = _plugins.find(pluginId);
    if (it == _plugins.end()) {
        qWarning() << "EditorPluginManager::reloadPlugin: Plugin" << pluginId << "not found";
        return false;
    }

    IEditorPlugin *plugin = it->second.get();
    if (!plugin->reload()) {
        qWarning() << "EditorPluginManager::reloadPlugin: Failed to reload plugin" << pluginId;
        return false;
    }

    // Re-register ViewModels
    if (_qml_engine) {
        plugin->register_view_models(_qml_engine);
    }

    qDebug() << "EditorPluginManager::reloadPlugin: Plugin" << pluginId << "reloaded";
    emit pluginReloaded(pluginId);

    return true;
}

void EditorPluginManager::unloadAllPlugins() {
    // Collect all plugin IDs (because unloadPlugin modifies the map)
    QStringList pluginIds;
    for (const auto &pair : _plugins) {
        pluginIds.append(pair.first);
    }
    for (const QString &pluginId : pluginIds) {
        unloadPlugin(pluginId);
    }
}

// === Plugin Query ===

IEditorPlugin *EditorPluginManager::getPlugin(const QString &id) const {
    auto it = _plugins.find(id);
    if (it != _plugins.end()) {
        return it->second.get();
    }
    return nullptr;
}

QList<IEditorPlugin *> EditorPluginManager::getLoadedPlugins() const {
    QList<IEditorPlugin *> result;
    for (auto it = _plugins.begin(); it != _plugins.end(); ++it) {
        result.append(it->second.get());
    }
    return result;
}

QList<IEditorPlugin *> EditorPluginManager::getPluginsByCategory(const QString &category) const {
    Q_UNUSED(category);
    return getLoadedPlugins();
}

// === Hot Reload Management ===

void EditorPluginManager::enableHotReload(bool enable) {
    _hot_reload_enabled = enable;
    if (enable && !_hot_reload_watcher) {
        _hot_reload_watcher = new QFileSystemWatcher(this);
        connect(_hot_reload_watcher, &QFileSystemWatcher::fileChanged,
                this, [](const QString &path) {
                    // TODO: Implement hot reload logic
                    qDebug() << "EditorPluginManager: File changed:" << path;
                });
    }
}

bool EditorPluginManager::isHostReloadEnabled() const {
    return _hot_reload_enabled;
}

void EditorPluginManager::watchPluginDirectory(const QString &path) {
    if (!_hot_reload_watcher) {
        _hot_reload_watcher = new QFileSystemWatcher(this);
    }
    _hot_reload_watcher->addPath(path);
    qDebug() << "EditorPluginManager::watchPluginDirectory: Watching" << path;
}

// === Service Management ===

void EditorPluginManager::registerService(const QString &serviceId, QObject *service) {
    if (!service) {
        qWarning() << "EditorPluginManager::registerService: service is null";
        return;
    }

    if (_services.contains(serviceId)) {
        qWarning() << "EditorPluginManager::registerService: Service" << serviceId << "already registered";
        // Don't overwrite existing service
        return;
    }

    _services[serviceId] = service;
    // Set parent to PluginManager so services are automatically cleaned up
    // But only if service doesn't already have a parent (to avoid reparenting issues)
    if (!service->parent()) {
        service->setParent(this);
    }
    qDebug() << "EditorPluginManager::registerService: Service" << serviceId << "registered";
}

QObject *EditorPluginManager::getService(const QString &serviceId) const {
    return _services.value(serviceId, nullptr);
}

void EditorPluginManager::clearServices() {
    qDebug() << "EditorPluginManager::clearServices: Clearing all service references";

    // At this point all plugins are unloaded, all ViewModels deleted
    // ViewModels explicitly disconnected from services during destruction
    //
    // Here we only need:
    // 1. Clear PluginManager's references to services
    // 2. Prevent PluginManager from accessing services deleted by the app during destruction

    // Process all pending events to ensure all deleteLater() objects are deleted
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // Clear reference map (do not access service objects, avoid any potential issues)
    _services.clear();

    qDebug() << "EditorPluginManager::clearServices: All service references cleared";
}

// === QML Engine ===

void EditorPluginManager::setQmlEngine(QQmlEngine *engine) {
    _qml_engine = engine;
    if (engine) {
        // Re-register all ViewModels for loaded plugins
        for (auto it = _plugins.begin(); it != _plugins.end(); ++it) {
            it->second->register_view_models(engine);
        }
    }
}

QQmlEngine *EditorPluginManager::qmlEngine() {
    return _qml_engine;
}

// === Private Methods ===

void EditorPluginManager::resolvePluginDependencies() {
    // TODO: Implement dependency resolution
    qDebug() << "EditorPluginManager::resolvePluginDependencies: Not implemented yet";
}

void EditorPluginManager::initializePlugin(IEditorPlugin *plugin) {
    if (!plugin) {
        return;
    }

    // Plugin is already initialized after load()
    // This method can be used for additional initialization if needed
    Q_UNUSED(plugin);
}

}// namespace rbc
