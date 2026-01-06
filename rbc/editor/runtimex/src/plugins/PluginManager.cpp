#include "RBCEditorRuntime/plugins/PluginManager.h"
#include "RBCEditorRuntime/plugins/IEditorPlugin.h"
#include "RBCEditorRuntime/plugins/PluginContext.h"

// services
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

bool EditorPluginManager::loadPlugin(const QString &pluginPath) {
    auto &inst = rbc::PluginManager::instance();
    try {
        inst.load_module(pluginPath.toStdString().c_str());
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
    plugin->setParent(this);

    // Register ViewModels
    if (qmlEngine_) {
        plugin->register_view_models(qmlEngine_);
    }

    // Initialize plugin
    initializePlugin(plugin);

    qDebug() << "EditorPluginManager::loadPlugin: Plugin" << pluginId << "loaded successfully";
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

    // Unload plugin
    if (!plugin->unload()) {
        qWarning() << "EditorPluginManager::unloadPlugin: Failed to unload plugin" << pluginId;
        return false;
    } else {
        auto &inst = rbc::PluginManager::instance();
        inst.unload_module(plugin->name().toStdString().c_str());
    }

    // Remove from map
    plugins_.remove(pluginId);
    plugin->deleteLater();

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

void EditorPluginManager::registerService(const QString &name, QObject *service) {
    if (!service) {
        qWarning() << "EditorPluginManager::registerService: service is null";
        return;
    }

    if (services_.contains(name)) {
        qWarning() << "EditorPluginManager::registerService: Service" << name << "already registered";
    }

    services_[name] = service;
    service->setParent(this);
    qDebug() << "EditorPluginManager::registerService: Service" << name << "registered";
}

QObject *EditorPluginManager::getService(const QString &name) const {
    return services_.value(name, nullptr);
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