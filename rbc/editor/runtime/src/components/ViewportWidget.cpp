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
#include <QEvent>
#include <QMouseEvent>

namespace rbc {

// ViewportContainerWidget 实现
ViewportContainerWidget::ViewportContainerWidget(RhiWindow *rhiWindow, QWidget *parent)
    : QWidget(parent), m_rhiWindow(rhiWindow) {
    // 使用 createWindowContainer 创建实际的容器
    m_innerContainer = QWidget::createWindowContainer(rhiWindow, this);
    m_innerContainer->setFocusPolicy(Qt::NoFocus);
    m_innerContainer->setMinimumSize(400, 300);
    m_innerContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 设置内部容器对鼠标事件透明，这样事件会传递到父widget（ViewportContainerWidget）
    // 然后我们可以在 ViewportContainerWidget 上处理拖动，并手动转发到 RhiWindow
    m_innerContainer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_innerContainer->setMouseTracking(true);
    
    // 安装事件过滤器到实际容器
    m_innerContainer->installEventFilter(this);
    
    // 设置布局
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_innerContainer);
    setLayout(layout);
    
    // 启用鼠标跟踪，这样即使鼠标没有按下也能接收到移动事件
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_MouseTracking, true);
}

void ViewportContainerWidget::mousePressEvent(QMouseEvent *event) {
    // 处理拖动起始位置
    if (event->button() == Qt::LeftButton) {
        // Check if we have selected entities
        bool hasSelection = false;
        auto *renderApp = EditorEngine::instance().getRenderApp();
        if (renderApp) {
            auto *visApp = dynamic_cast<VisApp *>(renderApp);
            if (visApp && !visApp->dragged_object_ids.empty()) {
                hasSelection = true;
            }
        }
        
        // If we have selection, allow drag without Ctrl; otherwise require Ctrl for drag selection
        bool shouldRecordDrag = hasSelection || (event->modifiers() & Qt::ControlModifier);
        if (shouldRecordDrag) {
            m_dragStartPos = event->pos();
        }
    }
    
    // 转发事件到 RhiWindow
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    
    QWidget::mousePressEvent(event);
}

void ViewportContainerWidget::mouseReleaseEvent(QMouseEvent *event) {
    // Reset drag start position on mouse release
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = QPoint();
    }
    
    // 转发事件到 RhiWindow
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    
    QWidget::mouseReleaseEvent(event);
}

void ViewportContainerWidget::mouseMoveEvent(QMouseEvent *event) {
    // 处理拖动检测
    if (event->buttons() & Qt::LeftButton) {
        // Check if we have selected entities
        bool hasSelection = false;
        auto *renderApp = EditorEngine::instance().getRenderApp();
        if (renderApp) {
            auto *visApp = dynamic_cast<VisApp *>(renderApp);
            if (visApp && !visApp->dragged_object_ids.empty()) {
                hasSelection = true;
            }
        }
        
        // If we have selection, allow drag without Ctrl; otherwise require Ctrl for drag selection
        bool shouldDrag = hasSelection || (event->modifiers() & Qt::ControlModifier);
        
        if (shouldDrag && !m_dragStartPos.isNull()) {
            QPoint delta = event->pos() - m_dragStartPos;
            // Start drag if moved more than 5 pixels
            if (delta.manhattanLength() > 5) {
                // 发出拖动请求信号
                emit dragRequested();
                m_dragStartPos = QPoint(); // Reset to prevent multiple drags
                return; // 不继续处理事件
            }
        }
    }
    
    // 转发事件到 RhiWindow
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    
    QWidget::mouseMoveEvent(event);
}

bool ViewportContainerWidget::eventFilter(QObject *obj, QEvent *event) {
    // 让其他事件正常处理
    return QWidget::eventFilter(obj, event);
}

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
    
    // 安装事件过滤器到 RhiWindow，以便拦截鼠标事件并处理拖动
    m_rhiWindow->installEventFilter(this);

    // 使用自定义容器类来处理鼠标事件和拖动
    m_container = new ViewportContainerWidget(m_rhiWindow, this);
    m_container->setMinimumSize(400, 300);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 连接拖动请求信号
    connect(m_container, &ViewportContainerWidget::dragRequested, this, &ViewportWidget::onDragRequested);
    
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

void ViewportWidget::wheelEvent(QWheelEvent *event) {
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }
    QWidget::wheelEvent(event);
}

void ViewportWidget::setEditorContext(class EditorContext *context) {
    m_editorContext = context;
    if (m_container) {
        m_container->setEditorContext(context);
    }
}

bool ViewportWidget::eventFilter(QObject *obj, QEvent *event) {
    // 拦截发送到 RhiWindow 的鼠标事件，处理拖动逻辑
    if (obj == m_rhiWindow) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            
            if (mouseEvent->button() == Qt::LeftButton) {
                // Check if we have selected entities
                bool hasSelection = false;
                auto *renderApp = EditorEngine::instance().getRenderApp();
                if (renderApp) {
                    auto *visApp = dynamic_cast<VisApp *>(renderApp);
                    if (visApp && !visApp->dragged_object_ids.empty()) {
                        hasSelection = true;
                    }
                }
                
                // If we have selection, allow drag without Ctrl; otherwise require Ctrl for drag selection
                bool shouldRecordDrag = hasSelection || (mouseEvent->modifiers() & Qt::ControlModifier);
                if (shouldRecordDrag && m_container) {
                    m_container->setDragStartPos(mouseEvent->pos());
                }
            }
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            
            if (mouseEvent->buttons() & Qt::LeftButton && m_container) {
                // Check if we have selected entities
                bool hasSelection = false;
                auto *renderApp = EditorEngine::instance().getRenderApp();
                if (renderApp) {
                    auto *visApp = dynamic_cast<VisApp *>(renderApp);
                    if (visApp && !visApp->dragged_object_ids.empty()) {
                        hasSelection = true;
                    }
                }
                
                // If we have selection, allow drag without Ctrl; otherwise require Ctrl for drag selection
                bool shouldDrag = hasSelection || (mouseEvent->modifiers() & Qt::ControlModifier);
                
                QPoint dragStartPos = m_container->dragStartPos();
                if (shouldDrag && !dragStartPos.isNull()) {
                    QPoint delta = mouseEvent->pos() - dragStartPos;
                    // Start drag if moved more than 5 pixels
                    if (delta.manhattanLength() > 5) {
                        // 启动拖动
                        startEntityDrag();
                        m_container->resetDragStartPos();
                        // 不阻止事件继续传播，让 RhiWindow 也能处理
                    }
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton && m_container) {
                m_container->resetDragStartPos();
            }
        }
    }
    
    // 让事件继续传播
    return QWidget::eventFilter(obj, event);
}

void ViewportWidget::onDragRequested() {
    startEntityDrag();
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
    drag->exec(Qt::CopyAction);
}

}// namespace rbc
