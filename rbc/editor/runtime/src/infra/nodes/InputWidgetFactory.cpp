#include "RBCEditorRuntime/infra/nodes/InputWidgetFactory.h"
#include "RBCEditorRuntime/infra/nodes/InputWidgetCreators.h"
#include <QDebug>
#include <mutex>
#include <cmath>

namespace rbc {

InputWidgetFactory &InputWidgetFactory::instance() {
    static InputWidgetFactory factory;
    static std::once_flag flag;
    std::call_once(flag, []() {
        factory.registerDefaultCreators();
    });
    return factory;
}

void InputWidgetFactory::registerCreator(std::unique_ptr<IInputWidgetCreator> creator) {
    _creators.push_back(std::move(creator));
}

QWidget *InputWidgetFactory::createWidget(const QJsonObject &inputDef, 
                                          const InputWidgetStyle &style,
                                          QWidget *parent) const {
    QString type = inputDef["type"].toString();
    
    // Find supported creator
    IInputWidgetCreator *creator = findCreator(type, inputDef);
    if (!creator) {
        qWarning() << "InputWidgetFactory: No creator found for type:" << type;
        return nullptr;
    }
    
    // Merge styles: first load style from inputDef, then merge with passed style
    InputWidgetStyle finalStyle = InputWidgetStyle::fromJson(inputDef);
    finalStyle = finalStyle.merge(style);
    
    return creator->createWidget(inputDef, finalStyle, parent);
}

QVariant InputWidgetFactory::getValue(QWidget *widget) const {
    if (!widget) {
        return QVariant();
    }
    
    // Try all creators, find one that can handle this widget
    for (const auto &creator : _creators) {
        QVariant value = creator->getValue(widget);
        if (value.isValid()) {
            return value;
        }
    }
    
    return QVariant();
}

bool InputWidgetFactory::setValue(QWidget *widget, const QVariant &value) const {
    if (!widget) {
        return false;
    }
    
    // Try all creators, find one that can handle this widget
    for (const auto &creator : _creators) {
        // First check if value can be retrieved (indicates it can handle this widget type)
        QVariant oldValue = creator->getValue(widget);
        if (oldValue.isValid()) {
            // Set new value
            creator->setValue(widget, value);
            // Verify setting was successful
            QVariant checkValue = creator->getValue(widget);
            if (checkValue.isValid()) {
                // For floats, allow small error
                if (checkValue.typeId() == QMetaType::Double || value.typeId() == QMetaType::Double) {
                    if (qAbs(checkValue.toDouble() - value.toDouble()) < 1e-6) {
                        return true;
                    }
                } else if (checkValue == value) {
                    return true;
                }
            }
        }
    }
    
    return false;
}

IInputWidgetCreator *InputWidgetFactory::findCreator(const QString &type, const QJsonObject &inputDef) const {
    // Search by registration order, later registrations have higher priority (allows override)
    for (auto it = _creators.rbegin(); it != _creators.rend(); ++it) {
        if ((*it)->supports(type, inputDef)) {
            return it->get();
        }
    }
    return nullptr;
}

void InputWidgetFactory::registerDefaultCreators() {
    // Note: EntityIdInputCreator should be registered before IntegerInputCreator
    // So entity_id will prefer EntityIdInputCreator
    registerCreator(std::make_unique<EntityIdInputCreator>());
    registerCreator(std::make_unique<BooleanInputCreator>());
    registerCreator(std::make_unique<StringInputCreator>());
    registerCreator(std::make_unique<IntegerInputCreator>());
    registerCreator(std::make_unique<NumberInputCreator>());
}

}// namespace rbc
