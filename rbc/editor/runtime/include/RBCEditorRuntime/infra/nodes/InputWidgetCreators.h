#pragma once

#include "IInputWidgetCreator.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>

namespace rbc {

/**
 * NumberInputCreator - Numeric input creator (supports float, number)
 */
class NumberInputCreator : public IInputWidgetCreator {
public:
    [[nodiscard]] QWidget *createWidget(const QJsonObject &inputDef,
                          const InputWidgetStyle &style,
                          QWidget *parent = nullptr) override;

    [[nodiscard]] QVariant getValue(QWidget *widget) const override;
    void setValue(QWidget *widget, const QVariant &value) const override;
    [[nodiscard]] bool supports(const QString &type, const QJsonObject &inputDef) const override;
    [[nodiscard]] QString name() const override { return "NumberInputCreator"; }
};

/**
 * IntegerInputCreator - Integer input creator (supports int, integer)
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
 * EntityIdInputCreator - Entity ID input creator (special handling for entity_id)
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
 * StringInputCreator - String input creator (supports string, text)
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
 * BooleanInputCreator - Boolean input creator (supports bool, boolean)
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
