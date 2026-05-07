#pragma once

#include <rbc_config.h>
#include <memory>
#include <QString>

namespace rbc {

class IEditorPlugin;

/**
 * @brief Plugin factory interface - responsible for creating plugin instances
 *
 * Uses the factory pattern to let PluginManager fully manage the lifecycle
 * of all plugins, avoiding lifetime chaos and double-destruction issues
 * caused by external pointer passing.
 */
class RBC_EDITOR_RUNTIME_API IPluginFactory {
public:
    virtual ~IPluginFactory() = default;
    
    /**
     * @brief Create a plugin instance
     * @return unique_ptr to the plugin instance, ownership transferred to the caller
     */
    [[nodiscard]] virtual std::unique_ptr<IEditorPlugin> create() = 0;

    /**
     * @brief Get the plugin ID (used for pre-registration queries)
     */
    [[nodiscard]] virtual QString pluginId() const = 0;

    /**
     * @brief Get the plugin name
     */
    [[nodiscard]] virtual QString pluginName() const = 0;
};

/**
 * @brief Template factory class - simplifies built-in plugin factory creation
 *
 * Usage:
 * @code
 * pluginManager.registerFactory(std::make_unique<PluginFactory<MyPlugin>>());
 * // or use the convenience method:
 * pluginManager.registerPlugin<MyPlugin>();
 * @endcode
 *
 * Plugin class T must provide the following static methods:
 * - static QString staticPluginId()
 * - static QString staticPluginName()
 */
template<typename T>
class PluginFactory : public IPluginFactory {
public:
    [[nodiscard]] std::unique_ptr<IEditorPlugin> create() override {
        return std::make_unique<T>();
    }

    [[nodiscard]] QString pluginId() const override {
        return T::staticPluginId();
    }

    [[nodiscard]] QString pluginName() const override {
        return T::staticPluginName();
    }
};

} // namespace rbc
