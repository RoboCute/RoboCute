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
 * @brief 编辑器插件管理器
 * 
 * 采用工厂模式统一管理所有插件的生命周期：
 * - 所有插件都通过工厂创建
 * - 所有插件都由 PluginManager 使用 unique_ptr 管理
 * - 消除了外部传入指针导致的生命周期混乱和双重析构问题
 */
class RBC_EDITOR_RUNTIME_API EditorPluginManager : public QObject {
    Q_OBJECT
public:
    static EditorPluginManager &instance();

    // === 工厂注册 ===

    /**
     * @brief 注册插件工厂
     * @param factory 插件工厂的 unique_ptr，所有权转移给 PluginManager
     */
    void registerFactory(std::unique_ptr<IPluginFactory> factory);

    /**
     * @brief 注册内置插件（模板便捷方法）
     * 
     * 使用方式：
     * @code
     * pluginManager.registerPlugin<ViewportPlugin>();
     * @endcode
     * 
     * 要求插件类 T 必须提供以下静态方法：
     * - static QString staticPluginId()
     * - static QString staticPluginName()
     */
    template<typename T>
    void registerPlugin() {
        registerFactory(std::make_unique<PluginFactory<T>>());
    }

    // === Plugin LifeCycle ===

    /**
     * @brief 从已注册的工厂创建并加载插件
     * @param pluginId 插件 ID（必须先通过 registerFactory 或 registerPlugin 注册）
     * @return 加载成功返回 true
     */
    bool loadPlugin(const QString &pluginId);

    /**
     * @brief 从动态库加载插件
     * @param pluginPath 动态库路径
     * @return 加载成功返回 true
     * 
     * 动态库必须导出 createPluginFactory() 函数
     */
    bool loadPluginFromDLL(const QString &pluginPath);

    /**
     * @brief 卸载插件
     * @param pluginId 插件 ID
     * @return 卸载成功返回 true
     */
    bool unloadPlugin(const QString &pluginId);

    /**
     * @brief 重新加载插件
     * @param pluginId 插件 ID
     * @return 重新加载成功返回 true
     */
    bool reloadPlugin(const QString &pluginId);

    /**
     * @brief 卸载所有已加载的插件
     */
    void unloadAllPlugins();

    // === Plugin Query ===

    /**
     * @brief 获取已加载的插件
     * @param id 插件 ID
     * @return 插件指针（不转移所有权），未找到返回 nullptr
     */
    IEditorPlugin *getPlugin(const QString &id) const;

    /**
     * @brief 获取所有已加载的插件
     */
    QList<IEditorPlugin *> getLoadedPlugins() const;

    /**
     * @brief 按分类获取插件
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
     * @brief 清理所有 service 引用
     * 
     * 必须在 services 的真实拥有者（如 QApplication）析构前调用
     * 这样可以避免 EditorPluginManager 析构时访问已被删除的 service
     */
    void clearServices();

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
     * @brief 内部加载插件实现
     * @param plugin 已创建的插件 unique_ptr
     * @param pluginId 插件 ID
     * @return 加载成功返回 true
     */
    bool loadPluginInternal(std::unique_ptr<IEditorPlugin> plugin, const QString &pluginId);

    // 使用 unique_ptr 管理插件生命周期，统一所有权
    // 注意：使用 std::map 而非 QMap，因为 QMap 不支持 move-only 类型
    std::map<QString, std::unique_ptr<IEditorPlugin>> plugins_;

    // 已注册的插件工厂
    std::map<QString, std::unique_ptr<IPluginFactory>> factories_;

    // 动态库模块（保持加载状态以防止插件代码被卸载）
    std::map<QString, luisa::shared_ptr<luisa::DynamicModule>> modules_;

    QMap<QString, QObject *> services_;
    QQmlEngine *qmlEngine_ = nullptr;
    QFileSystemWatcher *hotReloadWatcher_ = nullptr;
    bool hotReloadEnabled_ = false;
};

}// namespace rbc
