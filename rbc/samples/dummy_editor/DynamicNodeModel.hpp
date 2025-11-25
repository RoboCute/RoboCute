#pragma once

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeData>
#include <QJsonObject>
#include <QJsonArray>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QCheckBox>
#include <map>
#include <memory>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;

/**
 * Simple data type for passing values between nodes
 */
struct GenericData : public NodeData {
public:
    GenericData() = default;
    explicit GenericData(const QVariant &value, const QString &typeName = "generic")
        : m_value(value), m_typeName(typeName) {}

    NodeDataType type() const override { return NodeDataType{m_typeName, m_typeName}; }
    QVariant value() const { return m_value; }

private:
    QVariant m_value;
    QString m_typeName;
};

/**
 * Dynamic node model created from backend metadata
 */
struct DynamicNodeModel : public NodeDelegateModel {
    Q_OBJECT

public:
    DynamicNodeModel(const QJsonObject &metadata);
    ~DynamicNodeModel() override = default;

    // NodeDelegateModel interface
    QString caption() const override { return m_displayName; }
    QString name() const override { return m_nodeType; }
    bool captionVisible() const override { return true; }

    unsigned int nPorts(PortType portType) const override;
    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    QString portCaption(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    // Custom methods
    QString nodeType() const { return m_nodeType; }
    QString category() const { return m_category; }
    QJsonObject getInputValues() const;
    void setOutputValues(const QJsonObject &outputs);

private:
    void createInputWidgets();
    QWidget *createWidgetForInput(const QJsonObject &inputDef);
    void updateOutputData(PortIndex port, const QVariant &value, const QString &typeName);

    QString m_nodeType;
    QString m_displayName;
    QString m_category;
    QString m_description;

    QJsonArray m_inputs;
    QJsonArray m_outputs;

    std::map<PortIndex, std::shared_ptr<GenericData>> m_outputData;
    std::map<PortIndex, std::shared_ptr<NodeData>> m_inputData;

    QWidget *m_mainWidget;
    std::map<QString, QWidget *> m_inputWidgets;
};
