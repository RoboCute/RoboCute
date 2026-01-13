#include "RBCEditorRuntime/infra/nodes/DynamicNodeModel.h"
#include "RBCEditorRuntime/infra/nodes/EntityIdSpinBox.h"
#include <QFormLayout>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QBuffer>
#include <QDebug>

namespace rbc {

DynamicNodeModel::DynamicNodeModel(const QJsonObject &metadata) : m_mainWidget(nullptr) {
    m_nodeType = metadata["node_type"].toString();
    m_displayName = metadata["display_name"].toString();
    m_category = metadata["category"].toString();
    m_description = metadata["description"].toString();
    m_inputs = metadata["inputs"].toArray();
    m_outputs = metadata["outputs"].toArray();

    // Initialize output data
    for (int i = 0; i < m_outputs.size(); ++i) {
        m_outputData[i] = std::make_shared<GenericNodeData>();
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

QtNodes::NodeDataType DynamicNodeModel::dataType(PortType portType, PortIndex portIndex) const {
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

    return QString{};
}

std::shared_ptr<QtNodes::NodeData> DynamicNodeModel::outData(PortIndex port) {
    if (m_outputData.count(port)) {
        return m_outputData[port];
    }
    return nullptr;
}

void DynamicNodeModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) {
    m_inputData[portIndex] = data;
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
    QString name = inputDef["name"].toString();
    QVariant defaultValue = inputDef["default"].toVariant();

    // TODO: change to factory register

    if (type == "number" || type == "float") {
        auto spinBox = new QDoubleSpinBox();
        spinBox->setRange(-999999, 999999);
        spinBox->setValue(defaultValue.toDouble());
        spinBox->setMinimumWidth(80);
        return spinBox;
    } else if (type == "int" || type == "integer") {
        // Use EntityIdSpinBox for entity_id inputs to support drag and drop
        if (name == "entity_id") {
            auto spinBox = new EntityIdSpinBox();
            spinBox->setValue(defaultValue.toInt());
            spinBox->setMinimumWidth(80);
            return spinBox;
        } else {
            auto spinBox = new QSpinBox();
            spinBox->setRange(-999999, 999999);
            spinBox->setValue(defaultValue.toInt());
            spinBox->setMinimumWidth(80);
            return spinBox;
        }
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

        // TODO: Use Other Identifier
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
    for (int i = 0; i < m_outputs.size(); ++i) {
        QJsonObject outputDef = m_outputs[i].toObject();
        QString outputName = outputDef["name"].toString();
        QString outputType = outputDef["type"].toString();

        if (outputs.contains(outputName)) {
            QVariant value = outputs[outputName].toVariant();
            updateOutputData(i, value, outputType);
            updatePreview(outputs[outputName], outputName);
        }
    }
}

void DynamicNodeModel::updateOutputData(PortIndex port, const QVariant &value, const QString &typeName) {
    m_outputData[port] = std::make_shared<GenericNodeData>(value, typeName);
    emit dataUpdated(port);
}

QJsonObject DynamicNodeModel::save() const {
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
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

void DynamicNodeModel::updatePreview(const QJsonValue &previewData, const QString &outputName) {
    // Find the output definition to determine type
    QString outputType;
    for (int i = 0; i < m_outputs.size(); ++i) {
        QJsonObject outputDef = m_outputs[i].toObject();
        if (outputDef["name"].toString() == outputName) {
            outputType = outputDef["type"].toString().toLower();
            break;
        }
    }

    // Handle image preview
    if (outputType.contains("image") || outputType.contains("picture") || outputType.contains("img")) {
        QString imageData;

        if (previewData.isString()) {
            imageData = previewData.toString();
        } else if (previewData.isObject()) {
            QJsonObject obj = previewData.toObject();
            if (obj.contains("url")) {
                imageData = obj["url"].toString();
            } else if (obj.contains("data")) {
                imageData = obj["data"].toString();
            } else if (obj.contains("path")) {
                imageData = obj["path"].toString();
            }
        }

        if (!imageData.isEmpty()) {
            // Create or update preview widget
            QLabel *previewLabel = nullptr;
            if (m_previewWidgets.count(outputName)) {
                previewLabel = qobject_cast<QLabel *>(m_previewWidgets[outputName]);
            }

            if (!previewLabel) {
                previewLabel = new QLabel();
                previewLabel->setScaledContents(true);
                previewLabel->setMinimumSize(150, 150);
                previewLabel->setMaximumSize(300, 300);
                previewLabel->setAlignment(Qt::AlignCenter);
                previewLabel->setStyleSheet("QLabel { border: 1px solid gray; background-color: white; }");
                m_previewWidgets[outputName] = previewLabel;

                // Add to main widget layout
                if (m_mainWidget) {
                    auto formLayout = qobject_cast<QFormLayout *>(m_mainWidget->layout());
                    if (formLayout) {
                        // Add preview as a new row in the form layout
                        formLayout->addRow(QString("Preview (%1)").arg(outputName), previewLabel);
                    } else {
                        // If no layout exists, create a VBoxLayout
                        auto layout = new QVBoxLayout();
                        layout->addWidget(previewLabel);
                        m_mainWidget->setLayout(layout);
                    }
                }
            }

            // Load image
            QPixmap pixmap;
            bool loaded = false;

            // Try to load from URL or base64 data
            if (imageData.startsWith("http://") || imageData.startsWith("https://")) {
                // URL - would need async loading, for now skip
                // In production, use QNetworkAccessManager
                previewLabel->setText("Loading...");
            } else if (imageData.startsWith("data:image") || imageData.startsWith("base64,")) {
                // Base64 encoded image
                QString base64Data = imageData;
                if (base64Data.contains(",")) {
                    base64Data = base64Data.split(",").last();
                }
                QByteArray imageBytes = QByteArray::fromBase64(base64Data.toUtf8());
                loaded = pixmap.loadFromData(imageBytes);
            } else {
                // Try as file path or direct base64
                QByteArray imageBytes = QByteArray::fromBase64(imageData.toUtf8());
                if (!imageBytes.isEmpty()) {
                    loaded = pixmap.loadFromData(imageBytes);
                }
                if (!loaded) {
                    // Try as file path
                    loaded = pixmap.load(imageData);
                }
            }

            if (loaded && !pixmap.isNull()) {
                // Scale pixmap to fit preview size
                pixmap = pixmap.scaled(200, 200, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                previewLabel->setPixmap(pixmap);
            } else {
                previewLabel->setText("Preview\n(Unable to load)");
            }
        }
    }
}

}// namespace rbc