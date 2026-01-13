#pragma once
/**
 * Dynamic Node Model from Json
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
#include <QPixmap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
namespace rbc {

struct GenericNodeData : public QtNodes::NodeData {
public:
    GenericNodeData() = default;
    explicit GenericNodeData(const QVariant &value, const QString &typeName = "generic") : m_value(value), m_typeName(typeName) {}

    [[nodiscard]] QtNodes::NodeDataType type() const override { return QtNodes::NodeDataType{
        m_typeName, m_typeName}; }
    [[nodiscard]] QVariant value() const { return m_value; }

private:
    QVariant m_value;
    QString m_typeName;
};

struct DynamicNodeModel : public QtNodes::NodeDelegateModel {
    Q_OBJECT

    using PortType = QtNodes::PortType;
    using PortIndex = QtNodes::PortIndex;
    using NodeDataType = QtNodes::NodeDataType;
    using NodeData = QtNodes::NodeData;

public:
    explicit DynamicNodeModel(const QJsonObject &metadata);
    ~DynamicNodeModel() override = default;

    [[nodiscard]] QString caption() const override { return m_displayName; }
    [[nodiscard]] QString name() const override { return m_nodeType; }
    [[nodiscard]] bool captionVisible() const override { return true; }
    [[nodiscard]] unsigned int nPorts(PortType) const override;
    [[nodiscard]] NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    [[nodiscard]] QString portCaption(PortType portType, PortIndex portIndex) const override;

    std::shared_ptr<QtNodes::NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<QtNodes::NodeData> data, PortIndex portIndex) override;

    QWidget *embeddedWidget() override;

    [[nodiscard]] QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    // Custom Methods
    [[nodiscard]] QString nodeType() const { return m_nodeType; }
    [[nodiscard]] QString category() const { return m_category; }
    [[nodiscard]] QJsonObject getInputValues() const;
    void setOutputValues(const QJsonObject &outputs);
    void updatePreview(const QJsonValue &previewData, const QString &outputName);

private:
    void createInputWidgets();
    static QWidget *createWidgetForInput(const QJsonObject &inputDef);
    void updateOutputData(QtNodes::PortIndex port, const QVariant &value, const QString &typeName);

    QString m_nodeType;
    QString m_displayName;
    QString m_category;
    QString m_description;

    QJsonArray m_inputs;
    QJsonArray m_outputs;

    std::map<QtNodes::PortIndex, std::shared_ptr<NodeData>> m_outputData;
    std::map<QtNodes::PortIndex, std::shared_ptr<NodeData>> m_inputData;

    QWidget *m_mainWidget;
    std::map<QString, QWidget *> m_inputWidgets;
    std::map<QString, QWidget *> m_previewWidgets;
};

}// namespace rbc