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
    m_creators.push_back(std::move(creator));
}

QWidget *InputWidgetFactory::createWidget(const QJsonObject &inputDef, 
                                          const InputWidgetStyle &style,
                                          QWidget *parent) const {
    QString type = inputDef["type"].toString();
    
    // 查找支持的创建器
    IInputWidgetCreator *creator = findCreator(type, inputDef);
    if (!creator) {
        qWarning() << "InputWidgetFactory: No creator found for type:" << type;
        return nullptr;
    }
    
    // 合并样式：先从 inputDef 加载样式，再与传入的 style 合并
    InputWidgetStyle finalStyle = InputWidgetStyle::fromJson(inputDef);
    finalStyle = finalStyle.merge(style);
    
    return creator->createWidget(inputDef, finalStyle, parent);
}

QVariant InputWidgetFactory::getValue(QWidget *widget) const {
    if (!widget) {
        return QVariant();
    }
    
    // 尝试所有创建器，找到能处理该组件的
    for (const auto &creator : m_creators) {
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
    
    // 尝试所有创建器，找到能处理该组件的
    for (const auto &creator : m_creators) {
        // 先检查是否能获取值（说明能处理该组件类型）
        QVariant oldValue = creator->getValue(widget);
        if (oldValue.isValid()) {
            // 设置新值
            creator->setValue(widget, value);
            // 验证是否设置成功
            QVariant checkValue = creator->getValue(widget);
            if (checkValue.isValid()) {
                // 对于浮点数，允许小的误差
                if (checkValue.type() == QVariant::Double || value.type() == QVariant::Double) {
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
    // 按注册顺序查找，后注册的优先级更高（允许覆盖）
    for (auto it = m_creators.rbegin(); it != m_creators.rend(); ++it) {
        if ((*it)->supports(type, inputDef)) {
            return it->get();
        }
    }
    return nullptr;
}

void InputWidgetFactory::registerDefaultCreators() {
    // 注意：EntityIdInputCreator 应该在 IntegerInputCreator 之前注册
    // 这样 entity_id 会优先使用 EntityIdInputCreator
    registerCreator(std::make_unique<EntityIdInputCreator>());
    registerCreator(std::make_unique<BooleanInputCreator>());
    registerCreator(std::make_unique<StringInputCreator>());
    registerCreator(std::make_unique<IntegerInputCreator>());
    registerCreator(std::make_unique<NumberInputCreator>());
}

}// namespace rbc
