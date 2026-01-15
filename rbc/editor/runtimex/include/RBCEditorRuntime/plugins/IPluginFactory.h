#pragma once

#include <rbc_config.h>
#include <memory>
#include <QString>

namespace rbc {

class IEditorPlugin;

/**
 * @brief 插件工厂接口 - 负责创建插件实例
 * 
 * 使用工厂模式让 PluginManager 全权管理所有插件的生命周期，
 * 避免外部传入指针导致的生命周期混乱和双重析构问题。
 */
class RBC_EDITOR_RUNTIME_API IPluginFactory {
public:
    virtual ~IPluginFactory() = default;
    
    /**
     * @brief 创建插件实例
     * @return 插件实例的 unique_ptr，所有权转移给调用者
     */
    virtual std::unique_ptr<IEditorPlugin> create() = 0;
    
    /**
     * @brief 获取插件 ID（用于注册前查询）
     */
    virtual QString pluginId() const = 0;
    
    /**
     * @brief 获取插件名称
     */
    virtual QString pluginName() const = 0;
};

/**
 * @brief 模板工厂类 - 简化内置插件的工厂创建
 * 
 * 使用方式：
 * @code
 * pluginManager.registerFactory(std::make_unique<PluginFactory<MyPlugin>>());
 * // 或使用便捷方法：
 * pluginManager.registerPlugin<MyPlugin>();
 * @endcode
 * 
 * 要求插件类 T 必须提供以下静态方法：
 * - static QString staticPluginId()
 * - static QString staticPluginName()
 */
template<typename T>
class PluginFactory : public IPluginFactory {
public:
    std::unique_ptr<IEditorPlugin> create() override {
        return std::make_unique<T>();
    }
    
    QString pluginId() const override {
        return T::staticPluginId();
    }
    
    QString pluginName() const override {
        return T::staticPluginName();
    }
};

} // namespace rbc
