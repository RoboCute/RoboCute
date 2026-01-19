#pragma once

#include <QString>
#include <QJsonObject>
#include <QVariant>

namespace rbc {

/**
 * InputWidgetStyle - 输入组件样式配置
 * 
 * 支持通过 JSON 配置或代码设置组件的样式属性
 */
struct InputWidgetStyle {
    // 尺寸相关
    int minimumWidth = -1;
    int minimumHeight = -1;
    int maximumWidth = -1;
    int maximumHeight = -1;
    
    // 数值范围（用于 SpinBox）
    double minValue = -999999.0;
    double maxValue = 999999.0;
    double step = 1.0;
    int decimals = 2;  // 用于 DoubleSpinBox
    
    // 样式表
    QString styleSheet;
    
    // 占位符文本（用于 LineEdit）
    QString placeholderText;
    
    // 其他自定义属性
    QJsonObject customProperties;
    
    /**
     * 从 JSON 对象加载样式配置
     */
    static InputWidgetStyle fromJson(const QJsonObject &json) {
        InputWidgetStyle style;
        
        if (json.contains("style")) {
            QJsonObject styleObj = json["style"].toObject();
            
            if (styleObj.contains("min_width")) {
                style.minimumWidth = styleObj["min_width"].toInt();
            }
            if (styleObj.contains("min_height")) {
                style.minimumHeight = styleObj["min_height"].toInt();
            }
            if (styleObj.contains("max_width")) {
                style.maximumWidth = styleObj["max_width"].toInt();
            }
            if (styleObj.contains("max_height")) {
                style.maximumHeight = styleObj["max_height"].toInt();
            }
            
            if (styleObj.contains("min_value")) {
                style.minValue = styleObj["min_value"].toDouble();
            }
            if (styleObj.contains("max_value")) {
                style.maxValue = styleObj["max_value"].toDouble();
            }
            if (styleObj.contains("step")) {
                style.step = styleObj["step"].toDouble();
            }
            if (styleObj.contains("decimals")) {
                style.decimals = styleObj["decimals"].toInt();
            }
            
            if (styleObj.contains("style_sheet")) {
                style.styleSheet = styleObj["style_sheet"].toString();
            }
            if (styleObj.contains("placeholder")) {
                style.placeholderText = styleObj["placeholder"].toString();
            }
            
            if (styleObj.contains("custom")) {
                style.customProperties = styleObj["custom"].toObject();
            }
        }
        
        return style;
    }
    
    /**
     * 合并另一个样式配置（优先级：other > this）
     */
    InputWidgetStyle merge(const InputWidgetStyle &other) const {
        InputWidgetStyle result = *this;
        
        if (other.minimumWidth >= 0) result.minimumWidth = other.minimumWidth;
        if (other.minimumHeight >= 0) result.minimumHeight = other.minimumHeight;
        if (other.maximumWidth >= 0) result.maximumWidth = other.maximumWidth;
        if (other.maximumHeight >= 0) result.maximumHeight = other.maximumHeight;
        
        result.minValue = other.minValue;
        result.maxValue = other.maxValue;
        result.step = other.step;
        result.decimals = other.decimals;
        
        if (!other.styleSheet.isEmpty()) result.styleSheet = other.styleSheet;
        if (!other.placeholderText.isEmpty()) result.placeholderText = other.placeholderText;
        
        // 合并自定义属性
        for (auto it = other.customProperties.begin(); it != other.customProperties.end(); ++it) {
            result.customProperties[it.key()] = it.value();
        }
        
        return result;
    }
};

}// namespace rbc
