#pragma once

#include <QString>
#include <QList>

namespace rbc {

/**
 * EntityDragDropHelper - 实体拖放辅助类
 * 
 * 提供拖放相关的常量、MIME类型和工具函数
 */
class EntityDragDropHelper {
public:
    // MIME type for entity drag and drop
    static constexpr const char *MIME_TYPE = "application/x-robocute-entity";

    // Create MIME data key for entity ID
    static QString entityIdKey() { return QString("entity_id"); }

    // Create MIME data key for entity name
    static QString entityNameKey() { return QString("entity_name"); }

    // Create MIME data for entity drag
    static QString createMimeData(int entityId, const QString &entityName = QString());

    // Create MIME data for multiple entities drag
    static QString createMimeDataForGroup(const QList<int> &entityIds, const QList<QString> &entityNames = QList<QString>());

    // Parse entity ID from MIME data
    static int parseEntityId(const QString &mimeData);

    // Parse entity name from MIME data
    static QString parseEntityName(const QString &mimeData);

    // Parse multiple entity IDs from MIME data (for group drag)
    static QList<int> parseEntityIds(const QString &mimeData);

    // Parse multiple entity names from MIME data (for group drag)
    static QList<QString> parseEntityNames(const QString &mimeData);

    // Check if MIME data contains multiple entities
    static bool isGroupMimeData(const QString &mimeData);
};

}// namespace rbc
