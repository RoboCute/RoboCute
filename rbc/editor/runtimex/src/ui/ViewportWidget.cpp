#include "RBCEditorRuntime/ui/ViewportWidget.h"
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>

namespace rbc {

// ============================================================================
// ViewportContainerWidget Implementation
// ============================================================================

ViewportContainerWidget::ViewportContainerWidget(RhiWindow *rhiWindow, QWidget *parent)
    : QWidget(parent), m_rhiWindow(rhiWindow) {
    // 使用 createWindowContainer 创建实际的容器
    m_innerContainer = QWidget::createWindowContainer(rhiWindow, this);
    m_innerContainer->setFocusPolicy(Qt::NoFocus);
    m_innerContainer->setMinimumSize(400, 300);
    m_innerContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 设置内部容器对鼠标事件透明，这样事件会传递到父widget
    m_innerContainer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_innerContainer->setMouseTracking(true);

    // 安装事件过滤器
    m_innerContainer->installEventFilter(this);

    // 设置布局
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_innerContainer);
    setLayout(layout);

    // 启用鼠标跟踪
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_MouseTracking, true);
}

void ViewportContainerWidget::mousePressEvent(QMouseEvent *event) {
    // 处理拖动起始位置
    if (event->button() == Qt::LeftButton) {
        // 记录拖动起始位置（支持 Ctrl 键配合拖动）
        if (event->modifiers() & Qt::ControlModifier) {
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
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = QPoint();
    }

    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }

    QWidget::mouseReleaseEvent(event);
}

void ViewportContainerWidget::mouseMoveEvent(QMouseEvent *event) {
    // 处理拖动检测
    if ((event->buttons() & Qt::LeftButton) && !m_dragStartPos.isNull()) {
        QPoint delta = event->pos() - m_dragStartPos;
        // 移动超过 5 像素开始拖动
        if (delta.manhattanLength() > 5) {
            emit dragRequested();
            m_dragStartPos = QPoint();// 重置防止多次触发
            return;
        }
    }

    // 转发事件到 RhiWindow
    if (m_rhiWindow) {
        QCoreApplication::sendEvent(m_rhiWindow, event);
    }

    QWidget::mouseMoveEvent(event);
}

bool ViewportContainerWidget::eventFilter(QObject *obj, QEvent *event) {
    return QWidget::eventFilter(obj, event);
}

// ============================================================================
// ViewportWidget Implementation
// ============================================================================

ViewportWidget::ViewportWidget(IRenderer *renderer,
                               QRhi::Implementation graphicsApi,
                               QWidget *parent)
    : QWidget(parent), m_renderer(renderer), m_graphicsApi(graphicsApi) {
    setupUi();
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

ViewportWidget::~ViewportWidget() {
    // Release SwapChain first while window is valid
    if (m_rhiWindow && m_rhiWindow->handle()) {
        m_rhiWindow->releaseSwapChain();
    }
    if (m_container) {
        delete m_container;
        m_container = nullptr;
    }
    m_rhiWindow = nullptr;
}

void ViewportWidget::setupUi() {

    m_rhiWindow = new RhiWindow(m_graphicsApi);
    m_rhiWindow->renderer = m_renderer;
    // 安装事件过滤器到 RhiWindow
    m_rhiWindow->installEventFilter(this);

    // 使用自定义容器类来处理鼠标事件和拖动
    m_container = new ViewportContainerWidget(m_rhiWindow, this);
    m_container->setMinimumSize(400, 300);
    m_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // 连接拖动请求信号
    connect(m_container, &ViewportContainerWidget::dragRequested,
            this, &ViewportWidget::onDragRequested);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_container);

    setLayout(layout);
}

QString ViewportWidget::graphicsApiName() const {
    if (m_rhiWindow) {
        return m_rhiWindow->graphicsApiName();
    }
    return QString();
}

void ViewportWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

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

bool ViewportWidget::eventFilter(QObject *obj, QEvent *event) {
    // 拦截 RhiWindow 的鼠标事件，处理拖动逻辑
    if (obj == m_rhiWindow && m_container) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton &&
                (mouseEvent->modifiers() & Qt::ControlModifier)) {
                m_container->setDragStartPos(mouseEvent->pos());
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) &&
                !m_container->dragStartPos().isNull()) {
                QPoint delta = mouseEvent->pos() - m_container->dragStartPos();
                if (delta.manhattanLength() > 5) {
                    onDragRequested();
                    m_container->resetDragStartPos();
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_container->resetDragStartPos();
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ViewportWidget::onDragRequested() {
    emit entityDragRequested();
}

}// namespace rbc
