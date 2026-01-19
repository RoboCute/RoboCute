#pragma once

#include "IInputWidgetCreator.h"
#include <QString>
#include <QMap>
#include <memory>
#include <vector>

namespace rbc {

/**
 * InputWidgetFactory - 输入组件工厂
 * 
 * 管理所有输入组件创建器，支持注册、查找和创建组件
 */
class InputWidgetFactory {
public:
    /**
     * 获取单例实例
     */
    static InputWidgetFactory &instance();
    
    /**
     * 注册创建器
     * @param creator 创建器指针（工厂会接管所有权）
     */
    void registerCreator(std::unique_ptr<IInputWidgetCreator> creator);
    
    /**
     * 创建输入组件
     * @param inputDef 输入定义（JSON对象）
     * @param style 样式配置（可选，如果为空则使用默认样式）
     * @param parent 父组件
     * @return 创建的组件，失败返回 nullptr
     */
    QWidget *createWidget(const QJsonObject &inputDef, 
                          const InputWidgetStyle &style = InputWidgetStyle(),
                          QWidget *parent = nullptr) const;
    
    /**
     * 从组件中获取值
     * @param widget 组件指针
     * @return 组件的值，如果无法识别组件类型则返回无效的 QVariant
     */
    QVariant getValue(QWidget *widget) const;
    
    /**
     * 设置组件的值
     * @param widget 组件指针
     * @param value 要设置的值
     * @return 是否成功设置
     */
    bool setValue(QWidget *widget, const QVariant &value) const;
    
    /**
     * 查找支持指定类型的创建器
     * @param type 输入类型字符串
     * @param inputDef 完整的输入定义
     * @return 创建器指针，如果未找到返回 nullptr
     */
    IInputWidgetCreator *findCreator(const QString &type, const QJsonObject &inputDef) const;
    
    /**
     * 注册默认创建器（内置类型）
     */
    void registerDefaultCreators();
    
private:
    InputWidgetFactory() = default;
    ~InputWidgetFactory() = default;
    InputWidgetFactory(const InputWidgetFactory &) = delete;
    InputWidgetFactory &operator=(const InputWidgetFactory &) = delete;
    
    std::vector<std::unique_ptr<IInputWidgetCreator>> m_creators;
};

}// namespace rbc
