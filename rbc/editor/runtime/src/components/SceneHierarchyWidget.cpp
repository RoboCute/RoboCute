#include "RBCEditorRuntime/components/SceneHierarchyWidget.h"
#include "RBCEditorRuntime/runtime/EntityDragDropHelper.h"
#include <QHeaderView>
#include <QMimeData>
#include <QDrag>
#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QDebug>

namespace rbc {

SceneHierarchyWidget::SceneHierarchyWidget(QWidget *parent)
    : QTreeWidget(parent), currentScene_(nullptr) {

    // Setup tree widget
    setHeaderLabel("Scene Hierarchy");
    setSelectionMode(QAbstractItemView::ExtendedSelection); // Support Ctrl+click multi-selection
    setAlternatingRowColors(true);
    
    // Enable drag and drop
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);

    // Create root item
    rootItem_ = new QTreeWidgetItem(this, QStringList{"Scene Root"});
    rootItem_->setExpanded(true);
    rootItem_->setData(0, Qt::UserRole, -1);// -1 = root, not an entity

    // Connect selection signal
    connect(this, &QTreeWidget::itemSelectionChanged,
            this, &SceneHierarchyWidget::onItemSelectionChanged);
}

SceneHierarchyWidget::~SceneHierarchyWidget() = default;

void SceneHierarchyWidget::updateFromScene(const SceneSync *sceneSync) {
    if (!sceneSync) {
        return;
    }

    currentScene_ = sceneSync;
    buildEntityTree(sceneSync);
}

void SceneHierarchyWidget::buildEntityTree(const SceneSync *sceneSync) {
    // Clear existing children
    while (rootItem_->childCount() > 0) {
        delete rootItem_->takeChild(0);
    }

    // Add entities
    const auto &entities = sceneSync->entities();
    for (const auto &entity : entities) {
        addEntityItem(entity, rootItem_);
    }

    // Expand root
    rootItem_->setExpanded(true);
}

void SceneHierarchyWidget::addEntityItem(const SceneEntity &entity, QTreeWidgetItem *parent) {
    // Create entity item
    QString entityLabel = QString("%1 (ID: %2)")
                              .arg(QString::fromStdString(entity.name.c_str()))
                              .arg(entity.id);

    QTreeWidgetItem *entityItem = new QTreeWidgetItem(parent, QStringList{entityLabel});
    entityItem->setData(0, Qt::UserRole, entity.id);
    entityItem->setExpanded(true);

    // Add component children
    QTreeWidgetItem *transformItem = new QTreeWidgetItem(entityItem, QStringList{"Transform"});
    transformItem->setData(0, Qt::UserRole, -1);

    if (entity.has_render_component) {
        QString renderLabel = QString("Render (Mesh: %1)")
                                  .arg(entity.render_component.mesh_id);
        QTreeWidgetItem *renderItem = new QTreeWidgetItem(entityItem, QStringList{renderLabel});
        renderItem->setData(0, Qt::UserRole, -1);
    }
}

void SceneHierarchyWidget::onItemSelectionChanged() {
    auto selected = selectedItems();
    if (selected.isEmpty()) {
        return;
    }

    QTreeWidgetItem *item = selected.first();
    int entityId = item->data(0, Qt::UserRole).toInt();

    // Only emit if it's an actual entity (not root or component)
    if (entityId > 0) {
        emit entitySelected(entityId);
    }
}

void SceneHierarchyWidget::startDrag(Qt::DropActions supportedActions) {
    // Get all selected items
    QList<QTreeWidgetItem*> selectedItems = this->selectedItems();
    if (selectedItems.isEmpty()) {
        QTreeWidget::startDrag(supportedActions);
        return;
    }
    
    // Filter to only entity items (entityId > 0)
    QList<int> entityIds;
    QList<QString> entityNames;
    
    for (QTreeWidgetItem *item : selectedItems) {
        int entityId = item->data(0, Qt::UserRole).toInt();
        if (entityId > 0) {
            entityIds.append(entityId);
            
            // Get entity name
            QString entityName;
            if (currentScene_) {
                const auto *entity = currentScene_->getEntity(entityId);
                if (entity) {
                    entityName = QString::fromStdString(entity->name.c_str());
                }
            }
            entityNames.append(entityName.isEmpty() ? QString::number(entityId) : entityName);
        }
    }
    
    if (entityIds.isEmpty()) {
        // No valid entity items selected
        return;
    }
    
    // Create drag object
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();
    
    QString mimeDataStr;
    QString dragText;
    
    if (entityIds.size() == 1) {
        // Single entity - use original format for backward compatibility
        mimeDataStr = EntityDragDropHelper::createMimeData(entityIds[0], entityNames[0]);
        dragText = QString("Entity %1: %2").arg(entityIds[0]).arg(entityNames[0]);
    } else {
        // Multiple entities - use group format
        mimeDataStr = EntityDragDropHelper::createMimeDataForGroup(entityIds, entityNames);
        dragText = QString("%1 entities").arg(entityIds.size());
    }
    
    mimeData->setData(EntityDragDropHelper::MIME_TYPE, mimeDataStr.toUtf8());
    mimeData->setText(dragText);
    
    drag->setMimeData(mimeData);
    
    // Set drag pixmap
    QPixmap pixmap(120, entityIds.size() == 1 ? 30 : 40);
    pixmap.fill(Qt::lightGray);
    QPainter painter(&pixmap);
    painter.setPen(Qt::black);
    if (entityIds.size() == 1) {
        painter.drawText(pixmap.rect(), Qt::AlignCenter, QString("Entity %1").arg(entityIds[0]));
    } else {
        painter.drawText(pixmap.rect(), Qt::AlignCenter, QString("%1 entities").arg(entityIds.size()));
    }
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(60, entityIds.size() == 1 ? 15 : 20));
    
    // Execute drag
    Qt::DropAction dropAction = drag->exec(supportedActions);
    
    qDebug() << "SceneHierarchyWidget: Dragged" << entityIds.size() << "entity(ies) with action" << dropAction;
}

}// namespace rbc
