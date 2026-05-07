#pragma once

#include <QString>
#include <QList>

namespace rbc {

/**
 * EntityDragDropHelper - Entity drag-and-drop helper class
 *
 * Provides drag-and-drop related constants, MIME types, and utility functions
 */
class EntityDragDropHelper {
public:
    // MIME type for entity drag and drop
    static constexpr const char *MIME_TYPE = "application/x-robocute-entity";

    // Create MIME data key for entity ID
    [[nodiscard]] static QString entityIdKey() { return QString("entity_id"); }

    // Create MIME data key for entity name
    [[nodiscard]] static QString entityNameKey() { return QString("entity_name"); }

    // Create MIME data for entity drag
    [[nodiscard]] static QString createMimeData(int entityId, const QString &entityName = QString());

    // Create MIME data for multiple entities drag
    [[nodiscard]] static QString createMimeDataForGroup(const QList<int> &entityIds, const QList<QString> &entityNames = QList<QString>());

    // Parse entity ID from MIME data
    [[nodiscard]] static int parseEntityId(const QString &mimeData);

    // Parse entity name from MIME data
    [[nodiscard]] static QString parseEntityName(const QString &mimeData);

    // Parse multiple entity IDs from MIME data (for group drag)
    [[nodiscard]] static QList<int> parseEntityIds(const QString &mimeData);

    // Parse multiple entity names from MIME data (for group drag)
    [[nodiscard]] static QList<QString> parseEntityNames(const QString &mimeData);

    // Check if MIME data contains multiple entities
    [[nodiscard]] static bool isGroupMimeData(const QString &mimeData);
};

}// namespace rbc
