#include "NodeFactory.hpp"
#include "DynamicNodeModel.hpp"
#include <QDebug>

NodeFactory::NodeFactory()
    : m_registry(std::make_shared<NodeDelegateModelRegistry>()) {
}

void NodeFactory::registerNodesFromMetadata(const QJsonArray &nodesMetadata,
                                            std::shared_ptr<NodeDelegateModelRegistry> registry) {
    if (registry) {
        m_registry = registry;
    }

    for (const QJsonValue &value : nodesMetadata) {
        QJsonObject metadata = value.toObject();
        QString nodeType = metadata["node_type"].toString();
        QString category = metadata["category"].toString();

        if (nodeType.isEmpty()) {
            qWarning() << "Skipping node with empty node_type";
            continue;
        }

        // Store metadata for later reference
        m_nodeMetadata[nodeType] = metadata;

        // Register a lambda that creates DynamicNodeModel instances
        // The registry will call creator()->name() to get the unique node name,
        // which returns m_nodeType from DynamicNodeModel
        m_registry->registerModel<DynamicNodeModel>(
            [metadata]() { return std::make_unique<DynamicNodeModel>(metadata); },
            category);

        qDebug() << "Registered node:" << nodeType << "in category:" << category;
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
