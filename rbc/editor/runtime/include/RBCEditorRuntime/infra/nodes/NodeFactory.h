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

    void registerNodesFromMetadata(const QJsonArray &nodesMetadata, const std::shared_ptr<NodeDelegateModelRegistry> &registry);

    [[nodiscard]] std::shared_ptr<NodeDelegateModelRegistry> getRegistry() const { return _registry; }
    [[nodiscard]] QJsonObject getNodeMetadata(const QString &nodeType) const;
    [[nodiscard]] QMap<QString, QVector<QJsonObject>> getNodesByCategory() const;

private:
    std::shared_ptr<NodeDelegateModelRegistry> _registry;
    QMap<QString, QJsonObject> _node_metadata;
};

}// namespace rbc