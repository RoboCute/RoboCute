#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QQmlEngine>
#include <QFileSystemWatcher>
#include <memory>
#include <map>
#include "RBCEditorRuntime/services/IEventBus.h"
#include "RBCEditorRuntime/services/IService.h"
#include "RBCEditorRuntime/plugins/IPluginFactory.h"
#include <rbc_plugin/plugin_manager.h>

namespace rbc {

class IEditorPlugin;

/**
 * @brief Editor plugin manager
 * 
 * Manages all plugin lifecycles using the factory pattern:
 * - All plugins are created through factories
 * - All plugins are managed by PluginManager using unique_ptr
 * - Eliminates lifetime chaos and double-destruction from external pointers
 */
class RBC_EDITOR_RUNTIME_API EditorPluginManager : public QObject {
    Q_OBJECT
public:
    static EditorPluginManager &instance();

    // === Factory Registration ===

    /**
     * @brief Register a plugin factory
     * @param factory unique_ptr to the plugin factory, ownership transferred to PluginManager
     */
    void registerFactory(std::unique_ptr<IPluginFactory> factory);

    /**
     * @brief Register a built-in plugin (template convenience method)
     * 
     * Usage:
     * @code
     * pluginManager.registerPlugin<ViewportPlugin>();
     * @endcode
     * 
     * Plugin class T must provide the following static methods:
     * - static QString staticPluginId()
     * - static QString staticPluginName()
     */
    template<typename T>
    void registerPlugin() {
        registerFactory(std::make_unique<PluginFactory<T>>());
    }

    // === Plugin LifeCycle ===

    /**
     * @brief Create and load a plugin from a registered factory
     * @param pluginId Plugin ID (must be registered via registerFactory or registerPlugin first)
     * @return true if loaded successfully
     */
    bool loadPlugin(const QString &pluginId);

    /**
     * @brief Load a plugin from a dynamic library
     * @param pluginPath Dynamic library path
     * @return true if loaded successfully
     * 
     * Dynamic library must export createPluginFactory() function
     */
    bool loadPluginFromDLL(const QString &pluginPath);

    /**
     * @brief Unload a plugin
     * @param pluginId Plugin ID
     * @return true if unloaded successfully
     */
    bool unloadPlugin(const QString &pluginId);

    /**
     * @brief Reload a plugin
     * @param pluginId Plugin ID
     * @return true if reloaded successfully
     */
    bool reloadPlugin(const QString &pluginId);

    /**
     * @brief Unload all loaded plugins
     */
    void unloadAllPlugins();

    // === Plugin Query ===

    /**
     * @brief Get a loaded plugin
     * @param id Plugin ID
     * @return Plugin pointer (ownership not transferred), nullptr if not found
     */
    IEditorPlugin *getPlugin(const QString &id) const;

    /**
     * @brief Get all loaded plugins
     */
    QList<IEditorPlugin *> getLoadedPlugins() const;

    /**
     * @brief Get plugins by category
     */
    QList<IEditorPlugin *> getPluginsByCategory(const QString &category) const;

    // === Hot Reload Management ===
    void enableHotReload(bool enable);
    bool isHostReloadEnabled() const;
    void watchPluginDirectory(const QString &path);

    // === Service Management ===
    void registerService(const QString &serviceId, QObject *service);

    template<typename T>
    void registerService(T *service) {
        // Try to use IService::serviceId() if available
        if (auto *iservice = qobject_cast<IService *>(service)) {
            registerService(iservice->serviceId(), service);
        } else {
            // Fallback to class name for backward compatibility
            registerService(T::staticMetaObject.className(), service);
        }
    }

    QObject *getService(const QString &serviceId) const;

    /**
     * @brief Clear all service references
     * 
     * Must be called before the true owner of services (e.g., QApplication) is destroyed
     * This prevents EditorPluginManager from accessing deleted services during destruction
     */
    void clearServices();

    template<typename T>
    T *getService() const {
        // Try to find a registered service of type T and get its service ID
        QString serviceId;

        // First, try to find any registered service that can be cast to T
        for (auto it = _services.begin(); it != _services.end(); ++it) {
            QObject *service = it.value();
            if (qobject_cast<T *>(service)) {
                // If it's an IService, use its serviceId
                if (auto *iservice = qobject_cast<IService *>(service)) {
                    serviceId = iservice->serviceId();
                } else {
                    // Fallback to the key (which should be the serviceId)
                    serviceId = it.key();
                }
                return qobject_cast<T *>(service);
            }
        }

        // If not found by searching, try using class name as fallback
        serviceId = T::staticMetaObject.className();
        QObject *service = getService(serviceId);
        if (service) {
            // If found and it's an IService, update serviceId and re-get
            if (auto *iservice = qobject_cast<IService *>(service)) {
                QString correctId = iservice->serviceId();
                if (correctId != serviceId) {
                    service = getService(correctId);
                }
            }
            return qobject_cast<T *>(service);
        }

        return nullptr;
    }

    // === QML Engine ===
    void setQmlEngine(QQmlEngine *engine);
    QQmlEngine *qmlEngine();

signals:
    void pluginLoaded(const QString &pluginId);
    void pluginUnloaded(const QString &pluginId);
    void pluginReloaded(const QString &pluginId);
    void hotReloadTriggered(const QString &pluginId);

private:
    EditorPluginManager();
    ~EditorPluginManager();

    void resolvePluginDependencies();
    void initializePlugin(IEditorPlugin *plugin);

    /**
     * @brief Internal plugin load implementation
     * @param plugin Created plugin unique_ptr
     * @param pluginId Plugin ID
     * @return true if loaded successfully
     */
    bool loadPluginInternal(std::unique_ptr<IEditorPlugin> plugin, const QString &pluginId);

    // Manage plugin lifecycles with unique_ptr, unified ownership
    // Note: use std::map instead of QMap because QMap does not support move-only types
    std::map<QString, std::unique_ptr<IEditorPlugin>> _plugins;

    // Registered plugin factories
    std::map<QString, std::unique_ptr<IPluginFactory>> _factories;

    // Dynamic library modules (kept loaded to prevent plugin code unloading)
    std::map<QString, luisa::shared_ptr<luisa::DynamicModule>> _modules;

    QMap<QString, QObject *> _services;
    QQmlEngine *_qml_engine = nullptr;
    QFileSystemWatcher *_hot_reload_watcher = nullptr;
    bool _hot_reload_enabled = false;
};

}// namespace rbc
