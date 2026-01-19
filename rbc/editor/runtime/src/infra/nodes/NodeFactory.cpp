#include "RBCEditorRuntime/infra/nodes/NodeFactory.h"
#include "RBCEditorRuntime/infra/nodes/DynamicNodeModel.h"

namespace rbc {

NodeFactory::NodeFactory() : m_registry(std::make_shared<NodeDelegateModelRegistry>()) {
}

void NodeFactory::registerNodesFromMetadata(const QJsonArray &nodesMetadata, const std::shared_ptr<NodeDelegateModelRegistry> &registry) {
    if (registry) {
        m_registry = registry;
    }

    for (const auto &value : nodesMetadata) {
        QJsonObject metadata = value.toObject();
        QString nodeType = metadata["node_type"].toString();
        QString category = metadata["category"].toString();

        if (nodeType.isEmpty()) {
            qWarning() << "Skipping node with empty node_type";
            continue;
        }

        // store current metadata
        m_nodeMetadata[nodeType] = metadata;

        // Register a lambda that create DynamicNodeModel instances
        m_registry->registerModel<DynamicNodeModel>(
            [metadata]() { return std::make_unique<DynamicNodeModel>(metadata); }, category);

        qDebug() << "Registered node: " << nodeType << "in category: " << category;
    }
}

QJsonObject NodeFactory::getNodeMetadata(const QString &nodeType) const {
    return m_nodeMetadata.value(nodeType, QJsonObject());
}

QMap<QString, QVector<QJsonObject>> NodeFactory::getNodesByCategory() const {
    QMap<QString, QVector<QJsonObject>> result;

    for (auto it = m_nodeMetadata.begin(); it != m_nodeMetadata.end(); ++it) {
        QJsonObject metadata = it.value();
        QString category = metadata["category"].toString();
        result[category].append(metadata);
    }

    return result;
}

}// namespace rbc