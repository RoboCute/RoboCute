#pragma once

#include <QString>

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
    
    // Parse entity ID from MIME data
    static int parseEntityId(const QString &mimeData);
    
    // Parse entity name from MIME data
    static QString parseEntityName(const QString &mimeData);
};

}  // namespace rbc

