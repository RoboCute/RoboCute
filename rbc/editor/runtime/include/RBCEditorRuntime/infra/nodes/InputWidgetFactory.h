#pragma once

#include "IInputWidgetCreator.h"
#include <QString>
#include <QMap>
#include <memory>
#include <vector>

namespace rbc {

/**
 * InputWidgetFactory - Input widget factory
 *
 * Manages all input widget creators, supports registration, lookup, and creation
 */
class InputWidgetFactory {
public:
    /**
     * Get singleton instance
     */
    [[nodiscard]] static InputWidgetFactory &instance();
    
    /**
     * Register creator
     * @param creator Creator pointer (factory takes ownership)
     */
    void registerCreator(std::unique_ptr<IInputWidgetCreator> creator);
    
    /**
     * Create input widget
     * @param inputDef Input definition (JSON object)
     * @param style Style configuration (optional, uses default if empty)
     * @param parent Parent widget
     * @return Created widget, nullptr on failure
     */
    [[nodiscard]] QWidget *createWidget(const QJsonObject &inputDef,
                          const InputWidgetStyle &style = InputWidgetStyle(),
                          QWidget *parent = nullptr) const;
    
    /**
     * Get value from widget
     * @param widget Widget pointer
     * @return Widget value, invalid QVariant if widget type is not recognized
     */
    [[nodiscard]] QVariant getValue(QWidget *widget) const;
    
    /**
     * Set widget value
     * @param widget Widget pointer
     * @param value Value to set
     * @return Whether setting was successful
     */
    [[nodiscard]] bool setValue(QWidget *widget, const QVariant &value) const;
    
    /**
     * Find creator that supports the specified type
     * @param type Input type string
     * @param inputDef Complete input definition
     * @return Creator pointer, nullptr if not found
     */
    [[nodiscard]] IInputWidgetCreator *findCreator(const QString &type, const QJsonObject &inputDef) const;
    
    /**
     * Register default creators (built-in types)
     */
    void registerDefaultCreators();
    
private:
    InputWidgetFactory() = default;
    ~InputWidgetFactory() = default;
    InputWidgetFactory(const InputWidgetFactory &) = delete;
    InputWidgetFactory &operator=(const InputWidgetFactory &) = delete;
    
    std::vector<std::unique_ptr<IInputWidgetCreator>> _creators;
};

}// namespace rbc
