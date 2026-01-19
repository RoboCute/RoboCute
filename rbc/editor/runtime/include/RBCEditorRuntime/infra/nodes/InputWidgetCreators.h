#pragma once

#include "IInputWidgetCreator.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>

namespace rbc {

/**
 * NumberInputCreator - 数值输入创建器（支持 float, number）
 */
class NumberInputCreator : public IInputWidgetCreator {
public:
    QWidget *createWidget(const QJsonObject &inputDef, 
                          const InputWidgetStyle &style,
                          QWidget *parent = nullptr) override;
    
    QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) const override;
    bool supports(const QString &type, const QJsonObject &inputDef) const override;
    QString name() const override { return "NumberInputCreator"; }
};

/**
 * IntegerInputCreator - 整数输入创建器（支持 int, integer）
 */
class IntegerInputCreator : public IInputWidgetCreator {
public:
    QWidget *createWidget(const QJsonObject &inputDef, 
                          const InputWidgetStyle &style,
                          QWidget *parent = nullptr) override;
    
    QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) const override;
    bool supports(const QString &type, const QJsonObject &inputDef) const override;
    QString name() const override { return "IntegerInputCreator"; }
};

/**
 * EntityIdInputCreator - 实体ID输入创建器（特殊处理 entity_id）
 */
class EntityIdInputCreator : public IInputWidgetCreator {
public:
    QWidget *createWidget(const QJsonObject &inputDef, 
                          const InputWidgetStyle &style,
                          QWidget *parent = nullptr) override;
    
    QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) const override;
    bool supports(const QString &type, const QJsonObject &inputDef) const override;
    QString name() const override { return "EntityIdInputCreator"; }
};

/**
 * StringInputCreator - 字符串输入创建器（支持 string, text）
 */
class StringInputCreator : public IInputWidgetCreator {
public:
    QWidget *createWidget(const QJsonObject &inputDef, 
                          const InputWidgetStyle &style,
                          QWidget *parent = nullptr) override;
    
    QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) const override;
    bool supports(const QString &type, const QJsonObject &inputDef) const override;
    QString name() const override { return "StringInputCreator"; }
};

/**
 * BooleanInputCreator - 布尔值输入创建器（支持 bool, boolean）
 */
class BooleanInputCreator : public IInputWidgetCreator {
public:
    QWidget *createWidget(const QJsonObject &inputDef, 
                          const InputWidgetStyle &style,
                          QWidget *parent = nullptr) override;
    
    QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) const override;
    bool supports(const QString &type, const QJsonObject &inputDef) const override;
    QString name() const override { return "BooleanInputCreator"; }
};

}// namespace rbc
