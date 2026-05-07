#include "RBCEditorRuntime/infra/nodes/InputWidgetCreators.h"
#include "RBCEditorRuntime/infra/nodes/EntityIdSpinBox.h"
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDebug>

namespace rbc {

// NumberInputCreator implementation
QWidget *NumberInputCreator::createWidget(const QJsonObject &inputDef, 
                                         const InputWidgetStyle &style,
                                         QWidget *parent) {
    auto spinBox = new QDoubleSpinBox(parent);
    
    // Set range
    spinBox->setRange(style.minValue, style.maxValue);
    spinBox->setSingleStep(style.step);
    spinBox->setDecimals(style.decimals);
    
    // Set default value
    QVariant defaultValue = inputDef["default"].toVariant();
    spinBox->setValue(defaultValue.toDouble());
    
    // Set size
    if (style.minimumWidth >= 0) {
        spinBox->setMinimumWidth(style.minimumWidth);
    } else {
        spinBox->setMinimumWidth(80);
    }
    if (style.minimumHeight >= 0) {
        spinBox->setMinimumHeight(style.minimumHeight);
    }
    if (style.maximumWidth >= 0) {
        spinBox->setMaximumWidth(style.maximumWidth);
    }
    if (style.maximumHeight >= 0) {
        spinBox->setMaximumHeight(style.maximumHeight);
    }
    
    // Apply stylesheet
    if (!style.styleSheet.isEmpty()) {
        spinBox->setStyleSheet(style.styleSheet);
    }
    
    return spinBox;
}

QVariant NumberInputCreator::getValue(QWidget *widget) const {
    if (auto spinBox = qobject_cast<QDoubleSpinBox *>(widget)) {
        return spinBox->value();
    }
    return QVariant();
}

void NumberInputCreator::setValue(QWidget *widget, const QVariant &value) const {
    if (auto spinBox = qobject_cast<QDoubleSpinBox *>(widget)) {
        spinBox->setValue(value.toDouble());
    }
}

bool NumberInputCreator::supports(const QString &type, const QJsonObject &inputDef) const {
    Q_UNUSED(inputDef);
    return type == "number" || type == "float" || type == "double";
}

// IntegerInputCreator implementation
QWidget *IntegerInputCreator::createWidget(const QJsonObject &inputDef, 
                                          const InputWidgetStyle &style,
                                          QWidget *parent) {
    auto spinBox = new QSpinBox(parent);
    
    // Set range
    spinBox->setRange(static_cast<int>(style.minValue), static_cast<int>(style.maxValue));
    spinBox->setSingleStep(static_cast<int>(style.step));
    
    // Set default value
    QVariant defaultValue = inputDef["default"].toVariant();
    spinBox->setValue(defaultValue.toInt());
    
    // Set size
    if (style.minimumWidth >= 0) {
        spinBox->setMinimumWidth(style.minimumWidth);
    } else {
        spinBox->setMinimumWidth(80);
    }
    if (style.minimumHeight >= 0) {
        spinBox->setMinimumHeight(style.minimumHeight);
    }
    if (style.maximumWidth >= 0) {
        spinBox->setMaximumWidth(style.maximumWidth);
    }
    if (style.maximumHeight >= 0) {
        spinBox->setMaximumHeight(style.maximumHeight);
    }
    
    // Apply stylesheet
    if (!style.styleSheet.isEmpty()) {
        spinBox->setStyleSheet(style.styleSheet);
    }
    
    return spinBox;
}

QVariant IntegerInputCreator::getValue(QWidget *widget) const {
    if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
        return spinBox->value();
    }
    return QVariant();
}

void IntegerInputCreator::setValue(QWidget *widget, const QVariant &value) const {
    if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
        spinBox->setValue(value.toInt());
    }
}

bool IntegerInputCreator::supports(const QString &type, const QJsonObject &inputDef) const {
    // Exclude entity_id, it should be handled by EntityIdInputCreator
    QString name = inputDef["name"].toString();
    if (name == "entity_id") {
        return false;
    }
    return type == "int" || type == "integer";
}

// EntityIdInputCreator implementation
QWidget *EntityIdInputCreator::createWidget(const QJsonObject &inputDef, 
                                           const InputWidgetStyle &style,
                                           QWidget *parent) {
    auto spinBox = new EntityIdSpinBox(parent);
    
    // Set default value
    QVariant defaultValue = inputDef["default"].toVariant();
    spinBox->setValue(defaultValue.toInt());
    
    // Set size
    if (style.minimumWidth >= 0) {
        spinBox->setMinimumWidth(style.minimumWidth);
    } else {
        spinBox->setMinimumWidth(80);
    }
    if (style.minimumHeight >= 0) {
        spinBox->setMinimumHeight(style.minimumHeight);
    }
    if (style.maximumWidth >= 0) {
        spinBox->setMaximumWidth(style.maximumWidth);
    }
    if (style.maximumHeight >= 0) {
        spinBox->setMaximumHeight(style.maximumHeight);
    }
    
    // Apply stylesheet
    if (!style.styleSheet.isEmpty()) {
        spinBox->setStyleSheet(style.styleSheet);
    }
    
    return spinBox;
}

QVariant EntityIdInputCreator::getValue(QWidget *widget) const {
    if (auto spinBox = qobject_cast<EntityIdSpinBox *>(widget)) {
        return spinBox->value();
    }
    // Also supports plain QSpinBox (backward compatibility)
    if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
        return spinBox->value();
    }
    return QVariant();
}

void EntityIdInputCreator::setValue(QWidget *widget, const QVariant &value) const {
    if (auto spinBox = qobject_cast<EntityIdSpinBox *>(widget)) {
        spinBox->setValue(value.toInt());
    } else if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
        spinBox->setValue(value.toInt());
    }
}

bool EntityIdInputCreator::supports(const QString &type, const QJsonObject &inputDef) const {
    QString name = inputDef["name"].toString();
    return (type == "int" || type == "integer") && name == "entity_id";
}

// StringInputCreator implementation
QWidget *StringInputCreator::createWidget(const QJsonObject &inputDef, 
                                        const InputWidgetStyle &style,
                                        QWidget *parent) {
    auto lineEdit = new QLineEdit(parent);
    
    // Set default value
    QVariant defaultValue = inputDef["default"].toVariant();
    lineEdit->setText(defaultValue.toString());
    
    // Set placeholder
    if (!style.placeholderText.isEmpty()) {
        lineEdit->setPlaceholderText(style.placeholderText);
    }
    
    // Set size
    if (style.minimumWidth >= 0) {
        lineEdit->setMinimumWidth(style.minimumWidth);
    } else {
        lineEdit->setMinimumWidth(100);
    }
    if (style.minimumHeight >= 0) {
        lineEdit->setMinimumHeight(style.minimumHeight);
    }
    if (style.maximumWidth >= 0) {
        lineEdit->setMaximumWidth(style.maximumWidth);
    }
    if (style.maximumHeight >= 0) {
        lineEdit->setMaximumHeight(style.maximumHeight);
    }
    
    // Apply stylesheet
    if (!style.styleSheet.isEmpty()) {
        lineEdit->setStyleSheet(style.styleSheet);
    }
    
    return lineEdit;
}

QVariant StringInputCreator::getValue(QWidget *widget) const {
    if (auto lineEdit = qobject_cast<QLineEdit *>(widget)) {
        return lineEdit->text();
    }
    return QVariant();
}

void StringInputCreator::setValue(QWidget *widget, const QVariant &value) const {
    if (auto lineEdit = qobject_cast<QLineEdit *>(widget)) {
        lineEdit->setText(value.toString());
    }
}

bool StringInputCreator::supports(const QString &type, const QJsonObject &inputDef) const {
    Q_UNUSED(inputDef);
    return type == "string" || type == "text";
}

// BooleanInputCreator implementation
QWidget *BooleanInputCreator::createWidget(const QJsonObject &inputDef, 
                                          const InputWidgetStyle &style,
                                          QWidget *parent) {
    auto checkBox = new QCheckBox(parent);
    
    // Set default value
    QVariant defaultValue = inputDef["default"].toVariant();
    checkBox->setChecked(defaultValue.toBool());
    
    // Set size
    if (style.minimumWidth >= 0) {
        checkBox->setMinimumWidth(style.minimumWidth);
    }
    if (style.minimumHeight >= 0) {
        checkBox->setMinimumHeight(style.minimumHeight);
    }
    if (style.maximumWidth >= 0) {
        checkBox->setMaximumWidth(style.maximumWidth);
    }
    if (style.maximumHeight >= 0) {
        checkBox->setMaximumHeight(style.maximumHeight);
    }
    
    // Apply stylesheet
    if (!style.styleSheet.isEmpty()) {
        checkBox->setStyleSheet(style.styleSheet);
    }
    
    return checkBox;
}

QVariant BooleanInputCreator::getValue(QWidget *widget) const {
    if (auto checkBox = qobject_cast<QCheckBox *>(widget)) {
        return checkBox->isChecked();
    }
    return QVariant();
}

void BooleanInputCreator::setValue(QWidget *widget, const QVariant &value) const {
    if (auto checkBox = qobject_cast<QCheckBox *>(widget)) {
        checkBox->setChecked(value.toBool());
    }
}

bool BooleanInputCreator::supports(const QString &type, const QJsonObject &inputDef) const {
    Q_UNUSED(inputDef);
    return type == "bool" || type == "boolean";
}

}// namespace rbc
