#include "RBCEditorRuntime/nodes/EntityGroupListWidget.h"
#include "RBCEditorRuntime/runtime/EntityDragDropHelper.h"
#include <QMimeData>
#include <QPainter>
#include <QDebug>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>

namespace rbc {

EntityGroupListWidget::EntityGroupListWidget(QWidget *parent)
    : QWidget(parent),
      m_isDragOver(false) {
    
    // Create layout
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(4);
    
    // Create label
    m_label = new QLabel("Entities:", this);
    m_layout->addWidget(m_label);
    
    // Create list widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setMaximumHeight(150);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &EntityGroupListWidget::onItemDoubleClicked);
    m_layout->addWidget(m_listWidget);
    
    // Create clear button
    m_clearButton = new QPushButton("Clear All", this);
    connect(m_clearButton, &QPushButton::clicked, this, &EntityGroupListWidget::clearEntities);
    m_layout->addWidget(m_clearButton);
    
    setAcceptDrops(true);
    setMinimumHeight(200);
}

QList<int> EntityGroupListWidget::getEntityIds() const {
    return m_entityIds;
}

void EntityGroupListWidget::setEntityIds(const QList<int> &entityIds, const QList<QString> &entityNames) {
    m_entityIds = entityIds;
    m_entityNames = entityNames;
    updateEntityDisplay();
    emit entityListChanged(m_entityIds);
}

void EntityGroupListWidget::clearEntities() {
    m_entityIds.clear();
    m_entityNames.clear();
    updateEntityDisplay();
    emit entityListChanged(m_entityIds);
}

void EntityGroupListWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat(EntityDragDropHelper::MIME_TYPE)) {
        event->acceptProposedAction();
        m_isDragOver = true;
        update(); // Trigger repaint to show drag over state
    } else {
        event->ignore();
    }
}

void EntityGroupListWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasFormat(EntityDragDropHelper::MIME_TYPE)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void EntityGroupListWidget::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasFormat(EntityDragDropHelper::MIME_TYPE)) {
        QByteArray data = event->mimeData()->data(EntityDragDropHelper::MIME_TYPE);
        QString mimeDataStr = QString::fromUtf8(data);
        
        // Parse entity IDs
        QList<int> droppedIds = EntityDragDropHelper::parseEntityIds(mimeDataStr);
        QList<QString> droppedNames = EntityDragDropHelper::parseEntityNames(mimeDataStr);
        
        if (!droppedIds.isEmpty()) {
            // Add entities to list (avoid duplicates)
            for (int i = 0; i < droppedIds.size(); ++i) {
                int entityId = droppedIds[i];
                if (!m_entityIds.contains(entityId)) {
                    m_entityIds.append(entityId);
                    QString entityName = (i < droppedNames.size()) ? droppedNames[i] : QString::number(entityId);
                    m_entityNames.append(entityName);
                }
            }
            
            updateEntityDisplay();
            emit entityListChanged(m_entityIds);
            
            qDebug() << "EntityGroupListWidget: Added" << droppedIds.size() << "entity(ies), total:" << m_entityIds.size();
        }
        
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
    
    m_isDragOver = false;
    update(); // Trigger repaint to remove drag over state
}

void EntityGroupListWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);
    
    // Draw drag over indicator
    if (m_isDragOver) {
        QPainter painter(this);
        painter.setPen(QPen(QColor(0, 122, 204), 2, Qt::DashLine)); // #007acc with dash
        painter.setBrush(QBrush(QColor(0, 122, 204, 30))); // Semi-transparent blue
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 2, 2);
    }
}

void EntityGroupListWidget::updateEntityDisplay() {
    m_listWidget->clear();
    
    for (int i = 0; i < m_entityIds.size(); ++i) {
        int entityId = m_entityIds[i];
        QString entityName = (i < m_entityNames.size()) ? m_entityNames[i] : QString::number(entityId);
        
        QString displayText = QString("Entity %1: %2").arg(entityId).arg(entityName);
        QListWidgetItem *item = new QListWidgetItem(displayText, m_listWidget);
        item->setData(Qt::UserRole, entityId);
    }
    
    // Update label
    m_label->setText(QString("Entities (%1):").arg(m_entityIds.size()));
}

void EntityGroupListWidget::onRemoveEntity() {
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item) {
        return;
    }
    
    int entityId = item->data(Qt::UserRole).toInt();
    int index = m_entityIds.indexOf(entityId);
    
    if (index >= 0) {
        m_entityIds.removeAt(index);
        if (index < m_entityNames.size()) {
            m_entityNames.removeAt(index);
        }
        updateEntityDisplay();
        emit entityListChanged(m_entityIds);
    }
}

void EntityGroupListWidget::onItemDoubleClicked(QListWidgetItem *item) {
    // Double-click to remove entity
    onRemoveEntity();
}

}  // namespace rbc

