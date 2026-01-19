#include "RBCEditorRuntime/infra/events/EntityDragDropHelper.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace rbc {

QString EntityDragDropHelper::createMimeData(int entityId, const QString &entityName) {
    QJsonObject data;
    data[entityIdKey()] = entityId;
    if (!entityName.isEmpty()) {
        data[entityNameKey()] = entityName;
    }

    QJsonDocument doc(data);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

int EntityDragDropHelper::parseEntityId(const QString &mimeData) {
    QJsonDocument doc = QJsonDocument::fromJson(mimeData.toUtf8());
    if (!doc.isObject()) {
        return -1;
    }

    QJsonObject obj = doc.object();
    if (!obj.contains(entityIdKey())) {
        return -1;
    }

    return obj[entityIdKey()].toInt(-1);
}

QString EntityDragDropHelper::parseEntityName(const QString &mimeData) {
    QJsonDocument doc = QJsonDocument::fromJson(mimeData.toUtf8());
    if (!doc.isObject()) {
        return QString();
    }

    QJsonObject obj = doc.object();
    if (!obj.contains(entityNameKey())) {
        return QString();
    }

    return obj[entityNameKey()].toString();
}

QString EntityDragDropHelper::createMimeDataForGroup(const QList<int> &entityIds, const QList<QString> &entityNames) {
    QJsonObject data;

    // Add entity IDs array
    QJsonArray idsArray;
    for (int id : entityIds) {
        idsArray.append(id);
    }
    data["entity_ids"] = idsArray;

    // Add entity names array if provided
    if (!entityNames.isEmpty() && entityNames.size() == entityIds.size()) {
        QJsonArray namesArray;
        for (const QString &name : entityNames) {
            namesArray.append(name);
        }
        data["entity_names"] = namesArray;
    }

    // Mark as group data
    data["is_group"] = true;

    QJsonDocument doc(data);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QList<int> EntityDragDropHelper::parseEntityIds(const QString &mimeData) {
    QList<int> result;

    QJsonDocument doc = QJsonDocument::fromJson(mimeData.toUtf8());
    if (!doc.isObject()) {
        return result;
    }

    QJsonObject obj = doc.object();

    // Check if it's group data
    if (obj.contains("entity_ids")) {
        QJsonArray idsArray = obj["entity_ids"].toArray();
        for (const QJsonValue &val : idsArray) {
            result.append(val.toInt(-1));
        }
    } else if (obj.contains(entityIdKey())) {
        // Single entity format - return as list with one element
        result.append(obj[entityIdKey()].toInt(-1));
    }

    return result;
}

QList<QString> EntityDragDropHelper::parseEntityNames(const QString &mimeData) {
    QList<QString> result;

    QJsonDocument doc = QJsonDocument::fromJson(mimeData.toUtf8());
    if (!doc.isObject()) {
        return result;
    }

    QJsonObject obj = doc.object();

    // Check if it's group data
    if (obj.contains("entity_names")) {
        QJsonArray namesArray = obj["entity_names"].toArray();
        for (const QJsonValue &val : namesArray) {
            result.append(val.toString());
        }
    } else if (obj.contains(entityNameKey())) {
        // Single entity format - return as list with one element
        result.append(obj[entityNameKey()].toString());
    }

    return result;
}

bool EntityDragDropHelper::isGroupMimeData(const QString &mimeData) {
    QJsonDocument doc = QJsonDocument::fromJson(mimeData.toUtf8());
    if (!doc.isObject()) {
        return false;
    }

    QJsonObject obj = doc.object();
    return obj.contains("is_group") && obj["is_group"].toBool();
}

}// namespace rbc
