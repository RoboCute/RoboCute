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
    : QObject(nullptr), qmlEngine_(nullptr), hotReloadWatcher_(nullptr), hotReloadEnabled_(false) {
}

EditorPluginManager::~EditorPluginManager() {
    // 如果 clearServices() 没有被调用，在这里做最后的清理
    // 但此时如果 services 已被其 parent 删除，访问它们会导致 crash
    if (!services_.isEmpty()) {
        qWarning() << "EditorPluginManager::~EditorPluginManager: clearServices() was not called!";
        qWarning() << "This may cause crashes if services have already been destroyed by their parent.";
        // 不尝试访问 services，直接清空引用
        services_.clear();
    }

    // Unload all plugins
    unloadAllPlugins();

    // 清空工厂（在插件卸载后）
    factories_.clear();
}

// === 工厂注册 ===

void EditorPluginManager::registerFactory(std::unique_ptr<IPluginFactory> factory) {
    if (!factory) {
        qWarning() << "EditorPluginManager::registerFactory: factory is null";
        return;
    }

    QString id = factory->pluginId();
    if (factories_.find(id) != factories_.end()) {
        qWarning() << "EditorPluginManager::registerFactory: Factory for" << id << "already registered";
        return;
    }

    qDebug() << "EditorPluginManager::registerFactory: Registered factory for" << id
             << "(" << factory->pluginName() << ")";
    factories_[id] = std::move(factory);
}

// === Plugin LifeCycle ===

bool EditorPluginManager::loadPlugin(const QString &pluginId) {
    if (plugins_.find(pluginId) != plugins_.end()) {
        qWarning() << "EditorPluginManager::loadPlugin: Plugin" << pluginId << "already loaded";
        return false;
    }

    auto factoryIt = factories_.find(pluginId);
    if (factoryIt == factories_.end()) {
        qWarning() << "EditorPluginManager::loadPlugin: No factory registered for" << pluginId;
        return false;
    }

    // 通过工厂创建插件
    auto plugin = factoryIt->second->create();
    if (!plugin) {
        qWarning() << "EditorPluginManager::loadPlugin: Factory failed to create plugin" << pluginId;
        return false;
    }

    return loadPluginInternal(std::move(plugin), pluginId);
}

bool EditorPluginManager::loadPluginFromDLL(const QString &pluginPath) {
    auto &inst = rbc::PluginManager::instance();
    try {
        auto module = inst.load_module(pluginPath.toStdString().c_str());

        // 新设计：动态库导出 createPluginFactory 函数
        IPluginFactory *factoryPtr = module->invoke<IPluginFactory *()>("createPluginFactory");
        if (!factoryPtr) {
            qWarning() << "EditorPluginManager::loadPluginFromDLL: createPluginFactory returned null from" << pluginPath;
            return false;
        }

        // 接管工厂所有权
        std::unique_ptr<IPluginFactory> factory(factoryPtr);
        QString pluginId = factory->pluginId();

        // 通过工厂创建插件
        auto plugin = factory->create();
        if (!plugin) {
            qWarning() << "EditorPluginManager::loadPluginFromDLL: Factory failed to create plugin from" << pluginPath;
            return false;
        }

        plugin->plugin_path = pluginPath;

        if (!loadPluginInternal(std::move(plugin), pluginId)) {
            return false;
        }

        // 保持 DLL 模块加载状态，防止插件代码被卸载
        modules_[pluginId] = std::move(module);

        // 保存工厂以支持重新加载
        factories_[pluginId] = std::move(factory);

        return true;
    } catch (std::exception &e) {
        qWarning() << "EditorPluginManager::loadPluginFromDLL: Plugin from" << pluginPath
                   << "load failed:" << e.what();
        return false;
    }
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
    if (qmlEngine_) {
        plugin->register_view_models(qmlEngine_);
    }

    // Initialize plugin
    initializePlugin(plugin.get());

    QString pluginName = plugin->name();

    // 使用 unique_ptr 管理插件生命周期
    plugins_[pluginId] = std::move(plugin);

    qDebug() << "EditorPluginManager::loadPluginInternal: Plugin" << pluginId
             << "(" << pluginName << ") loaded successfully";
    emit pluginLoaded(pluginId);

    return true;
}

bool EditorPluginManager::unloadPlugin(const QString &pluginId) {
    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        qWarning() << "EditorPluginManager::unloadPlugin: Plugin" << pluginId << "not found";
        return false;
    }

    IEditorPlugin *plugin = it->second.get();

    // Check dependencies
    for (auto otherIt = plugins_.begin(); otherIt != plugins_.end(); ++otherIt) {
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

    // 保存 plugin_path 用于后续卸载 DLL
    QString pluginPath = plugin->plugin_path;
    auto moduleIt = modules_.find(pluginId);
    bool hasDynamicModule = (moduleIt != modules_.end());

    // 如果是动态库插件，保持模块加载以便调用虚函数
    luisa::shared_ptr<luisa::DynamicModule> moduleRef;
    if (hasDynamicModule) {
        moduleRef = moduleIt->second;
    }

    // 调用 unload（此时 DLL 仍然加载）
    if (!plugin->unload()) {
        qWarning() << "EditorPluginManager::unloadPlugin: Failed to unload plugin" << pluginId;
        return false;
    }

    // 从 map 中移除，unique_ptr 自动 delete 插件对象
    // 这必须在 DLL 卸载之前完成
    plugins_.erase(it);

    // 卸载 DLL 模块（此时插件对象已删除）
    if (hasDynamicModule) {
        modules_.erase(pluginId);

        auto &inst = rbc::PluginManager::instance();
        if (!pluginPath.isEmpty()) {
            inst.unload_module(pluginPath.toStdString().c_str());
        }

        // 同时移除对应的工厂（因为工厂代码也在 DLL 中）
        factories_.erase(pluginId);
    }

    qDebug() << "EditorPluginManager::unloadPlugin: Plugin" << pluginId << "unloaded";
    emit pluginUnloaded(pluginId);

    return true;
}

bool EditorPluginManager::reloadPlugin(const QString &pluginId) {
    auto it = plugins_.find(pluginId);
    if (it == plugins_.end()) {
        qWarning() << "EditorPluginManager::reloadPlugin: Plugin" << pluginId << "not found";
        return false;
    }

    IEditorPlugin *plugin = it->second.get();
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
    // 收集所有 plugin ID（因为 unloadPlugin 会修改 map）
    QStringList pluginIds;
    for (const auto &pair : plugins_) {
        pluginIds.append(pair.first);
    }
    for (const QString &pluginId : pluginIds) {
        unloadPlugin(pluginId);
    }
}

// === Plugin Query ===

IEditorPlugin *EditorPluginManager::getPlugin(const QString &id) const {
    auto it = plugins_.find(id);
    if (it != plugins_.end()) {
        return it->second.get();
    }
    return nullptr;
}

QList<IEditorPlugin *> EditorPluginManager::getLoadedPlugins() const {
    QList<IEditorPlugin *> result;
    for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
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

// === Service Management ===

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

void EditorPluginManager::clearServices() {
    qDebug() << "EditorPluginManager::clearServices: Clearing all service references";

    // 此时所有 plugin 已经 unload，所有 ViewModel 已经被删除
    // ViewModel 在析构时已经显式断开了与 service 的连接
    //
    // 这里只需要：
    // 1. 清空 PluginManager 对 service 的引用
    // 2. 防止 PluginManager 析构时访问已被 app 删除的 service

    // 处理所有待处理的事件，确保所有 deleteLater() 的对象已被删除
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    // 清空引用 map（不访问 service 对象，避免任何潜在问题）
    services_.clear();

    qDebug() << "EditorPluginManager::clearServices: All service references cleared";
}

// === QML Engine ===

void EditorPluginManager::setQmlEngine(QQmlEngine *engine) {
    qmlEngine_ = engine;
    if (engine) {
        // Re-register all ViewModels for loaded plugins
        for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
            it->second->register_view_models(engine);
        }
    }
}

QQmlEngine *EditorPluginManager::qmlEngine() {
    return qmlEngine_;
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
