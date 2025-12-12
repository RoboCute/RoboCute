#include "RBCEditorRuntime/runtime/EntityDragDropHelper.h"
#include <QJsonDocument>
#include <QJsonObject>

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

}  // namespace rbc

