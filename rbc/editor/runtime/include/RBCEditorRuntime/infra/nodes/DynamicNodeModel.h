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
    explicit GenericNodeData(const QVariant &value, const QString &typeName = "generic") : _value(value), _type_name(typeName) {}

    [[nodiscard]] QtNodes::NodeDataType type() const override { return QtNodes::NodeDataType{
        _type_name, _type_name}; }
    [[nodiscard]] QVariant value() const { return _value; }

private:
    QVariant _value;
    QString _type_name;
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

    [[nodiscard]] QString caption() const override { return _display_name; }
    [[nodiscard]] QString name() const override { return _node_type; }
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
    [[nodiscard]] QString nodeType() const { return _node_type; }
    [[nodiscard]] QString category() const { return _category; }
    [[nodiscard]] QJsonObject getInputValues() const;
    void setOutputValues(const QJsonObject &outputs);
    void updatePreview(const QJsonValue &previewData, const QString &outputName);

private:
    void createInputWidgets();
    QWidget *createWidgetForInput(const QJsonObject &inputDef);
    void updateOutputData(QtNodes::PortIndex port, const QVariant &value, const QString &typeName);

    QString _node_type;
    QString _display_name;
    QString _category;
    QString _description;

    QJsonArray _inputs;
    QJsonArray _outputs;

    std::map<QtNodes::PortIndex, std::shared_ptr<NodeData>> _output_data;
    std::map<QtNodes::PortIndex, std::shared_ptr<NodeData>> _input_data;

    QWidget *_main_widget;
    std::map<QString, QWidget *> _input_widgets;
    std::map<QString, QWidget *> _preview_widgets;
};

}// namespace rbc