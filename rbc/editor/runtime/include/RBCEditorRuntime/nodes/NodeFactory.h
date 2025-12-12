#pragma once

#include <QtNodes/NodeDelegateModelRegistry>
#include <QJsonArray>
#include <QJsonObject>
#include <memory>

/**
 * Factory for creating and registering dynamic node models from backend metadata
 */

namespace rbc {
using QtNodes::NodeDelegateModelRegistry;

struct NodeFactory {
public:
    NodeFactory();
    // Load nodes from backend metadata and register them
    void registerNodesFromMetadata(const QJsonArray &nodesMetadata,
                                   const std::shared_ptr<NodeDelegateModelRegistry> &registry);
    // Get the registry
    [[nodiscard]] std::shared_ptr<NodeDelegateModelRegistry> getRegistry() const { return m_registry; }
    [[nodiscard]] QJsonObject getNodeMetadata(const QString &nodeType) const;
    [[nodiscard]] QMap<QString, QVector<QJsonObject>> getNodesByCategory() const;
private:
    std::shared_ptr<NodeDelegateModelRegistry> m_registry;
    QMap<QString, QJsonObject> m_nodeMetadata;
};

}// namespace rbc