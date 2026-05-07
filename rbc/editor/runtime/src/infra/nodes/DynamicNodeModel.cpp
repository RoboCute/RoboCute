#include "RBCEditorRuntime/infra/nodes/DynamicNodeModel.h"
#include "RBCEditorRuntime/infra/nodes/InputWidgetFactory.h"
#include <QFormLayout>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QBuffer>
#include <QDebug>

namespace rbc {

DynamicNodeModel::DynamicNodeModel(const QJsonObject &metadata) : _main_widget(nullptr) {
    _node_type = metadata["node_type"].toString();
    _display_name = metadata["display_name"].toString();
    _category = metadata["category"].toString();
    _description = metadata["description"].toString();
    _inputs = metadata["inputs"].toArray();
    _outputs = metadata["outputs"].toArray();

    // Initialize output data
    for (int i = 0; i < _outputs.size(); ++i) {
        _output_data[i] = std::make_shared<GenericNodeData>();
    }
}

unsigned int DynamicNodeModel::nPorts(PortType portType) const {
    switch (portType) {
        case PortType::In:
            return static_cast<unsigned int>(_inputs.size());
        case PortType::Out:
            return static_cast<unsigned int>(_outputs.size());
        default:
            return 0;
    }
}

QtNodes::NodeDataType DynamicNodeModel::dataType(PortType portType, PortIndex portIndex) const {
    QString typeName = "generic";

    if (portType == PortType::In && portIndex < _inputs.size()) {
        typeName = _inputs[portIndex].toObject()["type"].toString();
    } else if (portType == PortType::Out && portIndex < _outputs.size()) {
        typeName = _outputs[portIndex].toObject()["type"].toString();
    }

    return NodeDataType{typeName, typeName};
}

QString DynamicNodeModel::portCaption(PortType portType, PortIndex portIndex) const {
    if (portType == PortType::In && portIndex < _inputs.size()) {
        QJsonObject input = _inputs[portIndex].toObject();
        return input["name"].toString();
    } else if (portType == PortType::Out && portIndex < _outputs.size()) {
        QJsonObject output = _outputs[portIndex].toObject();
        return output["name"].toString();
    }

    return QString{};
}

std::shared_ptr<QtNodes::NodeData> DynamicNodeModel::outData(PortIndex port) {
    if (_output_data.count(port)) {
        return _output_data[port];
    }
    return nullptr;
}

void DynamicNodeModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) {
    _input_data[portIndex] = data;
}

QWidget *DynamicNodeModel::embeddedWidget() {
    if (!_main_widget) {
        _main_widget = new QWidget();
        createInputWidgets();
    }
    return _main_widget;
}

void DynamicNodeModel::createInputWidgets() {
    auto layout = new QFormLayout(_main_widget);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(3);

    for (int i = 0; i < _inputs.size(); ++i) {
        QJsonObject inputDef = _inputs[i].toObject();
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
                _input_widgets[name] = widget;
            }
        }
    }

    _main_widget->setLayout(layout);
}

QWidget *DynamicNodeModel::createWidgetForInput(const QJsonObject &inputDef) {
    // Use factory pattern to create widgets, supports style config and dependency injection
    InputWidgetFactory &factory = InputWidgetFactory::instance();
    
    // Can read style config from inputDef, or use default style
    InputWidgetStyle style = InputWidgetStyle::fromJson(inputDef);
    
    return factory.createWidget(inputDef, style, _main_widget);
}

QJsonObject DynamicNodeModel::getInputValues() const {
    QJsonObject values;
    InputWidgetFactory &factory = InputWidgetFactory::instance();

    for (auto it = _input_widgets.begin(); it != _input_widgets.end(); ++it) {
        QString name = it->first;
        QWidget *widget = it->second;

        // Use factory to uniformly get value
        QVariant value = factory.getValue(widget);
        if (value.isValid()) {
            values[name] = QJsonValue::fromVariant(value);
        }
    }
    return values;
}

void DynamicNodeModel::setOutputValues(const QJsonObject &outputs) {
    for (int i = 0; i < _outputs.size(); ++i) {
        QJsonObject outputDef = _outputs[i].toObject();
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
    _output_data[port] = std::make_shared<GenericNodeData>(value, typeName);
    emit dataUpdated(port);
}

QJsonObject DynamicNodeModel::save() const {
    QJsonObject modelJson = QtNodes::NodeDelegateModel::save();
    modelJson["node_type"] = _node_type;
    modelJson["input_values"] = getInputValues();
    return modelJson;
}

void DynamicNodeModel::load(QJsonObject const &p) {
    if (p.contains("input_values")) {
        QJsonObject inputValues = p["input_values"].toObject();
        InputWidgetFactory &factory = InputWidgetFactory::instance();

        for (auto it = inputValues.begin(); it != inputValues.end(); ++it) {
            QString name = it.key();
            if (_input_widgets.count(name)) {
                QWidget *widget = _input_widgets[name];
                QVariant value = it.value().toVariant();
                if (!factory.setValue(widget, value)) {
                    qWarning() << "DynamicNodeModel::load: Failed to set value for input" << name;
                }
            }
        }
    }
}

void DynamicNodeModel::updatePreview(const QJsonValue &previewData, const QString &outputName) {
    // Find the output definition to determine type
    QString outputType;
    for (int i = 0; i < _outputs.size(); ++i) {
        QJsonObject outputDef = _outputs[i].toObject();
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
            if (_preview_widgets.count(outputName)) {
                previewLabel = qobject_cast<QLabel *>(_preview_widgets[outputName]);
            }

            if (!previewLabel) {
                previewLabel = new QLabel();
                previewLabel->setScaledContents(true);
                previewLabel->setMinimumSize(150, 150);
                previewLabel->setMaximumSize(300, 300);
                previewLabel->setAlignment(Qt::AlignCenter);
                previewLabel->setStyleSheet("QLabel { border: 1px solid gray; background-color: white; }");
                _preview_widgets[outputName] = previewLabel;

                // Add to main widget layout
                if (_main_widget) {
                    auto formLayout = qobject_cast<QFormLayout *>(_main_widget->layout());
                    if (formLayout) {
                        // Add preview as a new row in the form layout
                        formLayout->addRow(QString("Preview (%1)").arg(outputName), previewLabel);
                    } else {
                        // If no layout exists, create a VBoxLayout
                        auto layout = new QVBoxLayout();
                        layout->addWidget(previewLabel);
                        _main_widget->setLayout(layout);
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