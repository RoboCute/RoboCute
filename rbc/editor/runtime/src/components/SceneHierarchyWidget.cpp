#include "RBCEditorRuntime/components/SceneHierarchyWidget.h"
#include <QHeaderView>

namespace rbc {

SceneHierarchyWidget::SceneHierarchyWidget(QWidget *parent)
    : QTreeWidget(parent), currentScene_(nullptr) {

    // Setup tree widget
    setHeaderLabel("Scene Hierarchy");
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(true);

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

}// namespace rbc
