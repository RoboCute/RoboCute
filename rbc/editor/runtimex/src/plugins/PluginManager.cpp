#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"
#include "RBCEditorRuntime/services/ConnectionService.h"

#include <QDebug>
#include <QDir>

namespace rbc {

EditorPluginManager &EditorPluginManager::instance() {
    static EditorPluginManager mng;
    return mng;
}

EditorPluginManager::EditorPluginManager()
    : QObject(nullptr), qmlEngine_(nullptr), hotReloadWatcher_(nullptr), hotReloadEnabled_(false) {
}

EditorPluginManager::~EditorPluginManager() {
    // Stop all active operations in services before destruction
    for (auto it = services_.begin(); it != services_.end(); ++it) {
        QObject *service = it.value();
        if (service) {
            // Disconnect all signals to prevent callbacks during destruction
            service->disconnect();
            
            // If it's a ConnectionService, stop its timer and disconnect
            if (auto *connService = qobject_cast<ConnectionService *>(service)) {
                // Call ConnectionService::disconnect() to stop health check
                connService->disconnect();
            }
        }
    }
    
    // Unload all plugins before services are destroyed
    unloadAllPlugins();
    
    // Clear services map
    // Note: Services are children of their original parent (usually &app),
    // so they will be deleted when the app is destroyed, not here
    // We just clear our reference to them
    services_.clear();
}

bool EditorPluginManager::loadPlugin(const QString &pluginPath) {

    auto &inst = rbc::PluginManager::instance();
    try {
        auto module = inst.load_module(pluginPath.toStdString().c_str());
        IEditorPlugin *plugin = module->invoke<IEditorPlugin *()>("createPlugin");
        plugin->plugin_path = pluginPath;
        QString pluginId = plugin->id();
        if (!loadPlugin(plugin)) {
            return false;
        }
        // keep the module alive while the plugin instance exists
        modules_[pluginId] = std::move(module);
        return true;
    } catch (std::exception &e) {
        qFatal() << "Plugin from " << pluginPath << " load failed";
    }
    return false;
}

bool EditorPluginManager::loadPlugin(IEditorPlugin *plugin) {
    if (!plugin) {
        qWarning() << "EditorPluginManager::loadPlugin: plugin is null";
        return false;
    }

    QString pluginId = plugin->id();
    if (plugins_.contains(pluginId)) {
        qWarning() << "EditorPluginManager::loadPlugin: Plugin" << pluginId << "already loaded";
        return false;
    }

    // Create PluginContext
    PluginContext *context = new PluginContext(&instance(), this);

    // Load plugin
    if (!plugin->load(context)) {
        qWarning() << "EditorPluginManager::loadPlugin: Failed to load plugin" << pluginId;
        delete context;
        return false;
    }

    // Register plugin
    plugins_[pluginId] = plugin;
    // plugin->setParent(this);

    // Register ViewModels
    if (qmlEngine_) {
        plugin->register_view_models(qmlEngine_);
    }

    // Initialize plugin
    initializePlugin(plugin);

    qDebug() << "EditorPluginManager::loadPlugin: Plugin" << pluginId << "(" << plugin->name() << ") loaded successfully";
    emit pluginLoaded(pluginId);

    return true;
}

bool EditorPluginManager::unloadPlugin(const QString &pluginId) {
    if (!plugins_.contains(pluginId)) {
        qWarning() << "EditorPluginManager::unloadPlugin: Plugin" << pluginId << "not found";
        return false;
    }

    IEditorPlugin *plugin = plugins_[pluginId];
    // Check dependencies
    for (auto *otherPlugin : plugins_) {
        if (otherPlugin != plugin) {
            QStringList deps = otherPlugin->dependencies();
            if (deps.contains(pluginId)) {
                qWarning() << "EditorPluginManager::unloadPlugin: Cannot unload" << pluginId
                           << "because" << otherPlugin->id() << "depends on it";
                return false;
            }
        }
    }
    luisa::shared_ptr<luisa::DynamicModule> moduleRef;
    auto pluginPath = plugin->plugin_path;
    bool is_dynamic = plugin->is_dynamic();
    if (is_dynamic) {
        // if is dynamic plugin, safely unload dll
        auto &inst = rbc::PluginManager::instance();

        // keep DLL loaded while calling virtual methods
        if (modules_.contains(pluginId)) {
            moduleRef = modules_[pluginId];
        } else if (!pluginPath.isEmpty()) {
            moduleRef = inst.load_module(pluginPath.toStdString().c_str());
        }
    }

    // now it is safe to call virtual functions because the module stays loaded
    if (!plugin->unload()) {
        qWarning() << "EditorPluginManager::unloadPlugin: Failed to unload plugin" << pluginId;
        return false;
    }

    // 从 map 中移除（在删除对象之前）
    plugins_.remove(pluginId);

    // 同步删除对象（而不是 deleteLater），确保在 DLL 卸载前完成
    delete plugin;
    plugin = nullptr;

    // 最后卸载 DLL 模块（此时对象已删除，不再需要 DLL）
    if (is_dynamic) {
        modules_.remove(pluginId);
        auto &inst = rbc::PluginManager::instance();
        if (!pluginPath.isEmpty()) {
            inst.unload_module(pluginPath.toStdString().c_str());
        }
    }

    qDebug() << "EditorPluginManager::unloadPlugin: Plugin" << pluginId << "unloaded";
    emit pluginUnloaded(pluginId);

    return true;
}

bool EditorPluginManager::reloadPlugin(const QString &pluginId) {
    if (!plugins_.contains(pluginId)) {
        qWarning() << "EditorPluginManager::reloadPlugin: Plugin" << pluginId << "not found";
        return false;
    }

    IEditorPlugin *plugin = plugins_[pluginId];
    if (!plugin->reload()) {
        qWarning() << "EditorPluginManager::reloadPlugin: Failed to reload plugin" << pluginId;
        return false;
    }

    // Re-register ViewModels
    if (qmlEngine_) {
        plugin->register_view_models(qmlEngine_);
    }

    qDebug() << "EditorPluginManager::reloadPlugin: Plugin" << pluginId << "reloaded";
    emit pluginReloaded(pluginId);

    return true;
}

void EditorPluginManager::unloadAllPlugins() {
    QStringList pluginIds = plugins_.keys();
    for (const QString &pluginId : pluginIds) {
        unloadPlugin(pluginId);
    }
}

IEditorPlugin *EditorPluginManager::getPlugin(const QString &id) const {
    return plugins_.value(id, nullptr);
}

QList<IEditorPlugin *> EditorPluginManager::getLoadedPlugins() const {
    return plugins_.values();
}

QList<IEditorPlugin *> EditorPluginManager::getPluginsByCategory(const QString &category) const {
    // TODO: Implement category filtering
    Q_UNUSED(category);
    return plugins_.values();
}

void EditorPluginManager::enableHotReload(bool enable) {
    hotReloadEnabled_ = enable;
    if (enable && !hotReloadWatcher_) {
        hotReloadWatcher_ = new QFileSystemWatcher(this);
        connect(hotReloadWatcher_, &QFileSystemWatcher::fileChanged,
                this, [this](const QString &path) {
                    // TODO: Implement hot reload logic
                    qDebug() << "EditorPluginManager: File changed:" << path;
                });
    }
}

bool EditorPluginManager::isHostReloadEnabled() const {
    return hotReloadEnabled_;
}

void EditorPluginManager::watchPluginDirectory(const QString &path) {
    if (!hotReloadWatcher_) {
        hotReloadWatcher_ = new QFileSystemWatcher(this);
    }
    hotReloadWatcher_->addPath(path);
    qDebug() << "EditorPluginManager::watchPluginDirectory: Watching" << path;
}

void EditorPluginManager::registerService(const QString &serviceId, QObject *service) {
    if (!service) {
        qWarning() << "EditorPluginManager::registerService: service is null";
        return;
    }

    if (services_.contains(serviceId)) {
        qWarning() << "EditorPluginManager::registerService: Service" << serviceId << "already registered";
        // Don't overwrite existing service
        return;
    }

    services_[serviceId] = service;
    // Set parent to PluginManager so services are automatically cleaned up
    // But only if service doesn't already have a parent (to avoid reparenting issues)
    if (!service->parent()) {
        service->setParent(this);
    }
    qDebug() << "EditorPluginManager::registerService: Service" << serviceId << "registered";
}

QObject *EditorPluginManager::getService(const QString &serviceId) const {
    return services_.value(serviceId, nullptr);
}

void EditorPluginManager::setQmlEngine(QQmlEngine *engine) {
    qmlEngine_ = engine;
    if (engine) {
        // Re-register all ViewModels for loaded plugins
        for (auto *plugin : plugins_) {
            plugin->register_view_models(engine);
        }
    }
}

QQmlEngine *EditorPluginManager::qmlEngine() {
    return qmlEngine_;
}

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