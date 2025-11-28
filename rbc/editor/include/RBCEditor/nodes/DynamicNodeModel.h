#pragma once
/**
 * The Dynamic Node from semantic Json
 */

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

namespace rbc {

using QtNodes::NodeData;    // has type
using QtNodes::NodeDataType;// has id + name
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

    [[nodiscard]] NodeDataType type() const override { return NodeDataType{m_typeName, m_typeName}; }
    [[nodiscard]] QVariant value() const { return m_value; }

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
    explicit DynamicNodeModel(const QJsonObject &metadata);
    ~DynamicNodeModel() override = default;

    // NodeDelegateModel interface
    [[nodiscard]] QString caption() const override { return m_displayName; }
    [[nodiscard]] QString name() const override { return m_nodeType; }
    [[nodiscard]] bool captionVisible() const override { return true; }
    [[nodiscard]] unsigned int nPorts(PortType portType) const override;
    [[nodiscard]] NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    [[nodiscard]] QString portCaption(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    [[nodiscard]] QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    // Custom methods
    [[nodiscard]] QString nodeType() const { return m_nodeType; }
    [[nodiscard]] QString category() const { return m_category; }
    [[nodiscard]] QJsonObject getInputValues() const;
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

}// namespace rbc