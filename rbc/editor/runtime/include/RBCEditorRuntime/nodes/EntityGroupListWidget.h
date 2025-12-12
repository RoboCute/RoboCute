#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QPaintEvent>
#include <QLabel>

namespace rbc {

/**
 * EntityGroupListWidget - 支持多个实体拖放的列表控件
 * 
 * 用于 EntityGroupInputNode 的 entity_group 输入，支持从 SceneHierarchy 拖放多个实体
 */
class EntityGroupListWidget : public QWidget {
    Q_OBJECT

public:
    explicit EntityGroupListWidget(QWidget *parent = nullptr);
    
    // Get list of entity IDs
    QList<int> getEntityIds() const;
    
    // Set entity IDs (for programmatic updates)
    void setEntityIds(const QList<int> &entityIds, const QList<QString> &entityNames = QList<QString>());
    
    // Clear all entities
    void clearEntities();

signals:
    // Emitted when entity list changes
    void entityListChanged(const QList<int> &entityIds);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onRemoveEntity();
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    void updateEntityDisplay();
    void addEntityToList(int entityId, const QString &entityName);
    
    QVBoxLayout *m_layout;
    QLabel *m_label;
    QListWidget *m_listWidget;
    QPushButton *m_clearButton;
    bool m_isDragOver;
    
    QList<int> m_entityIds;
    QList<QString> m_entityNames;
};

}  // namespace rbc

