#pragma once

#include <QtNodes/NodeDelegateModelRegistry>
#include <QJsonArray>
#include <QJsonObject>
#include <luisa/vstl/memory.h>

namespace rbc {
using QtNodes::NodeDelegateModelRegistry;

struct NodeFactory {
public:
    NodeFactory();

    [[nodiscard]] void registerNodesFromMetadata(const QJsonArray &nodesMetadata, const std::shared_ptr<NodeDelegateModelRegistry> &registry);

    [[nodiscard]] std::shared_ptr<NodeDelegateModelRegistry> getRegistry() const { return m_registry; }
    [[nodiscard]] QJsonObject getNodeMetadata(const QString &nodeType) const;
    [[nodiscard]] QMap<QString, QVector<QJsonObject>> getNodesByCategory() const;

private:
    std::shared_ptr<NodeDelegateModelRegistry> m_registry;
    QMap<QString, QJsonObject> m_nodeMetadata;
};

}// namespace rbc