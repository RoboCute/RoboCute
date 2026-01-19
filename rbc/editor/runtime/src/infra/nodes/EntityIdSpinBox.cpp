#include "RBCEditorRuntime/infra/nodes/EntityIdSpinBox.h"
#include "RBCEditorRuntime/infra/events/EntityDragDropHelper.h"
#include <QMimeData>
#include <QPainter>
#include <QDebug>

namespace rbc {

EntityIdSpinBox::EntityIdSpinBox(QWidget *parent)
    : QSpinBox(parent),
      m_isDragOver(false) {
    setAcceptDrops(true);
    setMinimum(0);
    setMaximum(999999);
}

void EntityIdSpinBox::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat(EntityDragDropHelper::MIME_TYPE)) {
        event->acceptProposedAction();
        m_isDragOver = true;
        update();// Trigger repaint to show drag over state
    } else {
        event->ignore();
    }
}

void EntityIdSpinBox::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasFormat(EntityDragDropHelper::MIME_TYPE)) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void EntityIdSpinBox::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasFormat(EntityDragDropHelper::MIME_TYPE)) {
        QByteArray data = event->mimeData()->data(EntityDragDropHelper::MIME_TYPE);
        QString mimeDataStr = QString::fromUtf8(data);

        int entityId = EntityDragDropHelper::parseEntityId(mimeDataStr);
        if (entityId > 0) {
            setValue(entityId);
            qDebug() << "EntityIdSpinBox: Dropped entity ID" << entityId;
            emit valueChanged(entityId);// Emit signal to notify value change
        }

        event->acceptProposedAction();
    } else {
        event->ignore();
    }

    m_isDragOver = false;
    update();// Trigger repaint to remove drag over state
}

void EntityIdSpinBox::paintEvent(QPaintEvent *event) {
    QSpinBox::paintEvent(event);

    // Draw drag over indicator
    if (m_isDragOver) {
        QPainter painter(this);
        painter.setPen(QPen(QColor(0, 122, 204), 2, Qt::DashLine));// #007acc with dash
        painter.setBrush(QBrush(QColor(0, 122, 204, 30)));         // Semi-transparent blue
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 2, 2);
    }
}

}// namespace rbc
