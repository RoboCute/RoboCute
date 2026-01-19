#pragma once

#include <QWidget>
#include <QJsonObject>
#include <QVariant>
#include "InputWidgetStyle.h"

namespace rbc {

/**
 * IInputWidgetCreator - 输入组件创建器接口
 * 
 * 定义创建输入组件的统一接口，支持依赖注入和样式配置
 */
class IInputWidgetCreator {
public:
    virtual ~IInputWidgetCreator() = default;
    
    /**
     * 创建输入组件
     * @param inputDef 输入定义（JSON对象）
     * @param style 样式配置
     * @param parent 父组件
     * @return 创建的组件，失败返回 nullptr
     */
    virtual QWidget *createWidget(const QJsonObject &inputDef, 
                                   const InputWidgetStyle &style,
                                   QWidget *parent = nullptr) = 0;
    
    /**
     * 从组件中获取值
     * @param widget 组件指针
     * @return 组件的值
     */
    virtual QVariant getValue(QWidget *widget) const = 0;
    
    /**
     * 设置组件的值
     * @param widget 组件指针
     * @param value 要设置的值
     */
    virtual void setValue(QWidget *widget, const QVariant &value) const = 0;
    
    /**
     * 检查是否支持指定的输入类型
     * @param type 输入类型字符串
     * @param inputDef 完整的输入定义（可用于更复杂的判断）
     * @return 是否支持
     */
    virtual bool supports(const QString &type, const QJsonObject &inputDef) const = 0;
    
    /**
     * 获取创建器名称（用于调试和日志）
     */
    virtual QString name() const = 0;
};

}// namespace rbc
