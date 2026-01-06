#pragma once
#include <rbc_config.h>
#include <QObject>
#include <QQmlEngine>
#include <QFileSystemWatcher>
#include "RBCEditorRuntime/services/IEventBus.h"
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
    void registerService(const QString &name, QObject *service);
    QObject *getService(const QString &name) const;

    template<typename T>
    T *getService() const {
        return qobject_cast<T *>(getService(T::staticMetaObject.className()));
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

    void resolvePluginDependencies();
    void initializePlugin(IEditorPlugin *plugin);

    QMap<QString, IEditorPlugin *> plugins_;
    QMap<QString, QObject *> services_;
    QQmlEngine *qmlEngine_ = nullptr;
    QFileSystemWatcher *hotReloadWatcher_ = nullptr;
    bool hotReloadEnabled_ = false;
};

}// namespace rbc