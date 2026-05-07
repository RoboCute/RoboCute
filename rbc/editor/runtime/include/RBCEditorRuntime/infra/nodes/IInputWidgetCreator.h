#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QVariant>
#include "InputWidgetStyle.h"

namespace rbc {

/**
 * IInputWidgetCreator - Input widget creator interface
 *
 * Defines a unified interface for creating input widgets, supporting dependency injection and style configuration
 */
class IInputWidgetCreator {
public:
    virtual ~IInputWidgetCreator() = default;
    
    /**
     * Create input widget
     * @param inputDef Input definition (JSON object)
     * @param style Style configuration
     * @param parent Parent widget
     * @return Created widget, nullptr on failure
     */
    [[nodiscard]] virtual QWidget *createWidget(const QJsonObject &inputDef,
                                   const InputWidgetStyle &style,
                                   QWidget *parent = nullptr) = 0;
    
    /**
     * Get value from widget
     * @param widget Widget pointer
     * @return Widget value
     */
    [[nodiscard]] virtual QVariant getValue(QWidget *widget) const = 0;
    
    /**
     * Set widget value
     * @param widget Widget pointer
     * @param value Value to set
     */
    virtual void setValue(QWidget *widget, const QVariant &value) const = 0;
    
    /**
     * Check if the specified input type is supported
     * @param type Input type string
     * @param inputDef Complete input definition (can be used for more complex checks)
     * @return Whether supported
     */
    [[nodiscard]] virtual bool supports(const QString &type, const QJsonObject &inputDef) const = 0;
    
    /**
     * Get creator name (for debugging and logging)
     */
    [[nodiscard]] virtual QString name() const = 0;
};

}// namespace rbc
