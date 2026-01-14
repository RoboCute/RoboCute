#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QQmlEngine>
#include <QFileSystemWatcher>
#include "RBCEditorRuntime/services/IEventBus.h"
#include "RBCEditorRuntime/services/IService.h"
#include <rbc_plugin/plugin_manager.h>

namespace rbc {

struct IEditorPlugin;

class RBC_EDITOR_RUNTIME_API EditorPluginManager : public QObject {
    Q_OBJECT
public:
    static EditorPluginManager &instance();

    // Plugin LifeCycle
    bool loadPlugin(const QString &pluginPath);// Load From DLL
    bool loadPlugin(IEditorPlugin *plugin);    // Load from built-in instances
    bool unloadPlugin(const QString &pluginId);
    bool reloadPlugin(const QString &pluginId);
    void unloadAllPlugins();// 集中卸载全部Plugins

    // Plugin Query
    IEditorPlugin *getPlugin(const QString &id) const;
    QList<IEditorPlugin *> getLoadedPlugins() const;
    QList<IEditorPlugin *> getPluginsByCategory(const QString &category) const;

    // Hot Reload Management
    void enableHotReload(bool enable);
    bool isHostReloadEnabled() const;
    void watchPluginDirectory(const QString &path);

    // Register Services
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

    template<typename T>
    T *getService() const {
        // Try to find a registered service of type T and get its service ID
        QString serviceId;

        // First, try to find any registered service that can be cast to T
        for (auto it = services_.begin(); it != services_.end(); ++it) {
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

    // QML Engine
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

    QMap<QString, IEditorPlugin *> plugins_;
    QMap<QString, luisa::shared_ptr<luisa::DynamicModule>> modules_;

    QMap<QString, QObject *> services_;
    QQmlEngine *qmlEngine_ = nullptr;
    QFileSystemWatcher *hotReloadWatcher_ = nullptr;
    bool hotReloadEnabled_ = false;
};

}// namespace rbc