#include "RBCEditorRuntime/components/ViewportWidget.h"
#include "RBCEditorRuntime/engine/EditorEngine.h"
#include "RBCEditorRuntime/engine/visapp.h"
#include "RBCEditorRuntime/runtime/EntityDragDropHelper.h"
#include "RBCEditorRuntime/runtime/EditorContext.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "RBCEditorRuntime/runtime/SceneSyncManager.h"
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QMimeData>
#include <QDrag>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QDebug>

namespace rbc {

ViewportWidget::ViewportWidget(IRenderer *renderer, QWidget *parent)
    : QWidget(parent), m_renderer(renderer) {
    setupUi();
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

ViewportWidget::~ViewportWidget() {
    // Release SwapChain first while window is valid
    if (m_rhiWindow && m_rhiWindow->handle()) {
        m_rhiWindow->releaseSwapChain();
    }

    // CRITICAL: Destroy the native container widget.
    // According to Qt Docs for createWindowContainer: "The window container widget takes ownership of the QWindow window."
    // So deleting m_container AUTOMATICALLY deletes m_rhiWindow.
    // We must NOT delete m_rhiWindow manually afterwards, or it causes a Double Free crash.
    if (m_container) {
        delete m_container;
        m_container = nullptr;
    }

    m_rhiWindow = nullptr;
}

void ViewportWidget::setupUi() {
    // Get API from Engine or default
    auto api = EditorEngine::instance().getGraphicsApi();

    m_rhiWindow = new RhiWindow(api);
    m_rhiWindow->renderer = m_renderer;

    m_container = QWidget::createWindowContainer(m_rhiWindow, this);
    m_container->setFocusPolicy(Qt::NoFocus);
    m_container->setMinimumSize(400, 300);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_container);

    setLayout(layout);
}

void ViewportWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

// --- Event Forwarding ---

void ViewportWidget::keyPressEvent(QKeyEvent *event) {
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::keyPressEvent(event);
}

void ViewportWidget::keyReleaseEvent(QKeyEvent *event) {
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::keyReleaseEvent(event);
}

void ViewportWidget::mousePressEvent(QMouseEvent *event) {
    setFocus();// Ensure we get focus on click
    
    // Record drag start position for drag and drop
    if (event->button() == Qt::LeftButton && event->modifiers() & Qt::ControlModifier) {
        m_dragStartPos = event->pos();
    }
    
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::mousePressEvent(event);
}

void ViewportWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::mouseReleaseEvent(event);
}

void ViewportWidget::mouseMoveEvent(QMouseEvent *event) {
    // Check if we should start drag (Ctrl+LeftButton drag)
    if ((event->buttons() & Qt::LeftButton) && (event->modifiers() & Qt::ControlModifier)) {
        if (!m_dragStartPos.isNull()) {
            QPoint delta = event->pos() - m_dragStartPos;
            // Start drag if moved more than 5 pixels
            if (delta.manhattanLength() > 5) {
                startEntityDrag();
                m_dragStartPos = QPoint(); // Reset to prevent multiple drags
            }
        }
    }
    
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::mouseMoveEvent(event);
}

void ViewportWidget::wheelEvent(QWheelEvent *event) {
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::wheelEvent(event);
}

void ViewportWidget::startEntityDrag() {
    if (!m_editorContext || !m_editorContext->editorScene) {
        return;
    }
    
    // Get VisApp instance from EditorEngine
    auto *renderApp = EditorEngine::instance().getRenderApp();
    if (!renderApp) {
        return;
    }
    
    auto *visApp = dynamic_cast<VisApp *>(renderApp);
    if (!visApp || visApp->dragged_object_ids.empty()) {
        return;
    }
    
    // Convert instance ids to entity ids
    luisa::vector<int> entityIds = m_editorContext->editorScene->getEntityIdsFromInstanceIds(visApp->dragged_object_ids);
    if (entityIds.empty()) {
        return;
    }
    
    int entityId = entityIds[0]; // Use first selected entity
    
    // Get entity name from SceneSync
    QString entityName;
    if (m_editorContext->sceneSyncManager) {
        const auto *sceneSync = m_editorContext->sceneSyncManager->sceneSync();
        if (sceneSync) {
            const auto *entity = sceneSync->getEntity(entityId);
            if (entity) {
                // Convert luisa::string to QString
                entityName = QString::fromUtf8(entity->name.c_str(), static_cast<int>(entity->name.size()));
            }
        }
    }
    
    // Create drag object
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();
    
    // Create MIME data
    QString mimeDataStr = EntityDragDropHelper::createMimeData(entityId, entityName);
    mimeData->setData(EntityDragDropHelper::MIME_TYPE, mimeDataStr.toUtf8());
    mimeData->setText(QString("Entity %1: %2").arg(entityId).arg(entityName.isEmpty() ? QString::number(entityId) : entityName));
    
    drag->setMimeData(mimeData);
    
    // Set drag pixmap
    QPixmap pixmap(100, 30);
    pixmap.fill(Qt::lightGray);
    QPainter painter(&pixmap);
    painter.setPen(Qt::black);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QString("Entity %1").arg(entityId));
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(50, 15));
    
    // Execute drag
    Qt::DropAction dropAction = drag->exec(Qt::CopyAction);
    
    qDebug() << "ViewportWidget: Dragged entity" << entityId << "with action" << dropAction;
}

}// namespace rbc
