#pragma once

#include <QtNodes/NodeDelegateModelRegistry>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

using QtNodes::NodeDelegateModelRegistry;

/**
 * Factory for creating and registering dynamic node models from backend metadata
 */
struct NodeFactory
{
public:
    NodeFactory();

    // Load nodes from backend metadata and register them
    void registerNodesFromMetadata(const QJsonArray &nodesMetadata, 
                                   std::shared_ptr<NodeDelegateModelRegistry> registry);

    // Get the registry
    std::shared_ptr<NodeDelegateModelRegistry> getRegistry() const { return m_registry; }

    // Get node metadata by type
    QJsonObject getNodeMetadata(const QString &nodeType) const;
    
    // Get all node metadata grouped by category
    QMap<QString, QVector<QJsonObject>> getNodesByCategory() const;

private:
    std::shared_ptr<NodeDelegateModelRegistry> m_registry;
    QMap<QString, QJsonObject> m_nodeMetadata;
};

