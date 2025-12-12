#pragma once

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include "RBCEditorRuntime/runtime/SceneSync.h"

namespace rbc {

/**
 * Scene Hierarchy Widget
 * 
 * Displays the scene entity tree from synchronized scene state.
 * Updates when scene changes from server.
 */
class SceneHierarchyWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit SceneHierarchyWidget(QWidget *parent = nullptr);
    ~SceneHierarchyWidget() override;

    // Update the tree from scene sync data
    void updateFromScene(const SceneSync *sceneSync);

signals:
    // Emitted when an entity is selected in the tree
    void entitySelected(int entityId);

protected:
    // Override to enable drag
    void startDrag(Qt::DropActions supportedActions) override;

private slots:
    void onItemSelectionChanged();

private:
    void buildEntityTree(const SceneSync *sceneSync);
    void addEntityItem(const SceneEntity &entity, QTreeWidgetItem *parent);

    QTreeWidgetItem *rootItem_{};
    const SceneSync *currentScene_{};
};

}// namespace rbc
