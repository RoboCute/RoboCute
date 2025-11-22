#include "DynamicNodeModel.hpp"
#include <QFormLayout>
#include <QJsonDocument>

DynamicNodeModel::DynamicNodeModel(const QJsonObject &metadata)
    : m_mainWidget(nullptr) {
    m_nodeType = metadata["node_type"].toString();
    m_displayName = metadata["display_name"].toString();
    m_category = metadata["category"].toString();
    m_description = metadata["description"].toString();
    m_inputs = metadata["inputs"].toArray();
    m_outputs = metadata["outputs"].toArray();

    // Initialize output data
    for (int i = 0; i < m_outputs.size(); ++i) {
        m_outputData[i] = std::make_shared<GenericData>();
    }
}

unsigned int DynamicNodeModel::nPorts(PortType portType) const {
    switch (portType) {
        case PortType::In:
            return static_cast<unsigned int>(m_inputs.size());
        case PortType::Out:
            return static_cast<unsigned int>(m_outputs.size());
        default:
            return 0;
    }
}

NodeDataType DynamicNodeModel::dataType(PortType portType, PortIndex portIndex) const {
    QString typeName = "generic";

    if (portType == PortType::In && portIndex < m_inputs.size()) {
        typeName = m_inputs[portIndex].toObject()["type"].toString();
    } else if (portType == PortType::Out && portIndex < m_outputs.size()) {
        typeName = m_outputs[portIndex].toObject()["type"].toString();
    }

    return NodeDataType{typeName, typeName};
}

QString DynamicNodeModel::portCaption(PortType portType, PortIndex portIndex) const {
    if (portType == PortType::In && portIndex < m_inputs.size()) {
        QJsonObject input = m_inputs[portIndex].toObject();
        return input["name"].toString();
    } else if (portType == PortType::Out && portIndex < m_outputs.size()) {
        QJsonObject output = m_outputs[portIndex].toObject();
        return output["name"].toString();
    }

    return QString();
}

std::shared_ptr<NodeData> DynamicNodeModel::outData(PortIndex port) {
    if (m_outputData.count(port)) {
        return m_outputData[port];
    }
    return nullptr;
}

void DynamicNodeModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) {
    m_inputData[portIndex] = data;

    // For nodes with connections, we don't use the widget values
    // The connected data takes precedence
}

QWidget *DynamicNodeModel::embeddedWidget() {
    if (!m_mainWidget) {
        m_mainWidget = new QWidget();
        createInputWidgets();
    }
    return m_mainWidget;
}

void DynamicNodeModel::createInputWidgets() {
    auto layout = new QFormLayout(m_mainWidget);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(3);

    for (int i = 0; i < m_inputs.size(); ++i) {
        QJsonObject inputDef = m_inputs[i].toObject();
        QString name = inputDef["name"].toString();

        // Only create widgets for inputs that don't require connections
        // (i.e., inputs with default values that can be set directly)
        bool required = inputDef["required"].toBool(false);
        bool hasDefault = inputDef.contains("default");

        if (hasDefault || !required) {
            QWidget *widget = createWidgetForInput(inputDef);
            if (widget) {
                QString label = name;
                if (required) {
                    label += "*";
                }
                layout->addRow(label, widget);
                m_inputWidgets[name] = widget;
            }
        }
    }

    m_mainWidget->setLayout(layout);
}

QWidget *DynamicNodeModel::createWidgetForInput(const QJsonObject &inputDef) {
    QString type = inputDef["type"].toString();
    QVariant defaultValue = inputDef["default"].toVariant();

    if (type == "number" || type == "float") {
        auto spinBox = new QDoubleSpinBox();
        spinBox->setRange(-999999, 999999);
        spinBox->setValue(defaultValue.toDouble());
        spinBox->setMinimumWidth(80);

        // Note: Widget changes don't directly trigger output updates
        // They only affect the static input values used during execution

        return spinBox;
    } else if (type == "int" || type == "integer") {
        auto spinBox = new QSpinBox();
        spinBox->setRange(-999999, 999999);
        spinBox->setValue(defaultValue.toInt());
        spinBox->setMinimumWidth(80);

        return spinBox;
    } else if (type == "string" || type == "text") {
        auto lineEdit = new QLineEdit();
        lineEdit->setText(defaultValue.toString());
        lineEdit->setMinimumWidth(100);

        return lineEdit;
    } else if (type == "bool" || type == "boolean") {
        auto checkBox = new QCheckBox();
        checkBox->setChecked(defaultValue.toBool());

        return checkBox;
    }

    return nullptr;
}

QJsonObject DynamicNodeModel::getInputValues() const {
    QJsonObject values;

    for (auto it = m_inputWidgets.begin(); it != m_inputWidgets.end(); ++it) {
        QString name = it->first;
        QWidget *widget = it->second;

        if (auto spinBox = qobject_cast<QDoubleSpinBox *>(widget)) {
            values[name] = spinBox->value();
        } else if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
            values[name] = spinBox->value();
        } else if (auto lineEdit = qobject_cast<QLineEdit *>(widget)) {
            values[name] = lineEdit->text();
        } else if (auto checkBox = qobject_cast<QCheckBox *>(widget)) {
            values[name] = checkBox->isChecked();
        }
    }

    return values;
}

void DynamicNodeModel::setOutputValues(const QJsonObject &outputs) {
    // Update output data based on execution results
    for (int i = 0; i < m_outputs.size(); ++i) {
        QJsonObject outputDef = m_outputs[i].toObject();
        QString outputName = outputDef["name"].toString();
        QString outputType = outputDef["type"].toString();

        if (outputs.contains(outputName)) {
            QVariant value = outputs[outputName].toVariant();
            updateOutputData(i, value, outputType);
        }
    }
}

void DynamicNodeModel::updateOutputData(PortIndex port, const QVariant &value, const QString &typeName) {
    m_outputData[port] = std::make_shared<GenericData>(value, typeName);
    emit dataUpdated(port);
}

QJsonObject DynamicNodeModel::save() const {
    QJsonObject modelJson = NodeDelegateModel::save();
    modelJson["node_type"] = m_nodeType;
    modelJson["input_values"] = getInputValues();
    return modelJson;
}

void DynamicNodeModel::load(QJsonObject const &p) {
    if (p.contains("input_values")) {
        QJsonObject inputValues = p["input_values"].toObject();

        for (auto it = inputValues.begin(); it != inputValues.end(); ++it) {
            QString name = it.key();
            if (m_inputWidgets.count(name)) {
                QWidget *widget = m_inputWidgets[name];

                if (auto spinBox = qobject_cast<QDoubleSpinBox *>(widget)) {
                    spinBox->setValue(it.value().toDouble());
                } else if (auto spinBox = qobject_cast<QSpinBox *>(widget)) {
                    spinBox->setValue(it.value().toInt());
                } else if (auto lineEdit = qobject_cast<QLineEdit *>(widget)) {
                    lineEdit->setText(it.value().toString());
                } else if (auto checkBox = qobject_cast<QCheckBox *>(widget)) {
                    checkBox->setChecked(it.value().toBool());
                }
            }
        }
    }
}
