#include "RBCEditorRuntime/ui/ViewportWidget.h"
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>
#include <luisa/runtime/context.h>

namespace rbc {

// ============================================================================
// ViewportContainerWidget Implementation
// ============================================================================

ViewportContainerWidget::ViewportContainerWidget(RhiWindow *rhiWindow, QWidget *parent)
    : QWidget(parent), _rhi_window(rhiWindow) {
    // Use createWindowContainer to create actual container
    _inner_container = QWidget::createWindowContainer(rhiWindow, this);
    _inner_container->setFocusPolicy(Qt::NoFocus);
    _inner_container->setMinimumSize(400, 300);
    _inner_container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Set inner container transparent to mouse events so they pass to parent widget
    _inner_container->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    _inner_container->setMouseTracking(true);

    // Install event filter
    _inner_container->installEventFilter(this);

    // Set layout
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_inner_container);
    setLayout(layout);

    // Enable mouse tracking
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_MouseTracking, true);
}

void ViewportContainerWidget::mousePressEvent(QMouseEvent *event) {
    // Handle drag start position
    if (event->button() == Qt::LeftButton) {
        // Record drag start position (supports Ctrl+drag)
        if (event->modifiers() & Qt::ControlModifier) {
            _drag_start_pos = event->pos();
        }
    }

    // Forward events to RhiWindow
    if (_rhi_window) {
        QCoreApplication::sendEvent(_rhi_window, event);
    }

    QWidget::mousePressEvent(event);
}

void ViewportContainerWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        _drag_start_pos = QPoint();
    }

    if (_rhi_window) {
        QCoreApplication::sendEvent(_rhi_window, event);
    }

    QWidget::mouseReleaseEvent(event);
}

void ViewportContainerWidget::mouseMoveEvent(QMouseEvent *event) {
    // Handle drag detection
    if ((event->buttons() & Qt::LeftButton) && !_drag_start_pos.isNull()) {
        QPoint delta = event->pos() - _drag_start_pos;
        // Start dragging after moving more than 5 pixels
        if (delta.manhattanLength() > 5) {
            emit dragRequested();
            _drag_start_pos = QPoint();// Reset to prevent multiple triggers
            return;
        }
    }

    // Forward events to RhiWindow
    if (_rhi_window) {
        QCoreApplication::sendEvent(_rhi_window, event);
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
    : QWidget(parent), _renderer(renderer), _graphics_api(graphicsApi) {
    setupUi();
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

ViewportWidget::~ViewportWidget() {
    // Release SwapChain first while window is valid
    if (_rhi_window && _rhi_window->handle()) {
        _rhi_window->releaseSwapChain();
    }
    if (_container) {
        delete _container;
        _container = nullptr;
    }
    _rhi_window = nullptr;
}

void ViewportWidget::setupUi() {

    _rhi_window = new RhiWindow(_graphics_api);
    _rhi_window->renderer = _renderer;
    // Install event filter on RhiWindow
    _rhi_window->installEventFilter(this);

    // Use custom container class to handle mouse events and dragging
    _container = new ViewportContainerWidget(_rhi_window, this);
    _container->setMinimumSize(400, 300);
    _container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Connect drag request signal
    connect(_container, &ViewportContainerWidget::dragRequested,
            this, &ViewportWidget::onDragRequested);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(_container);

    setLayout(layout);
}

QString ViewportWidget::graphicsApiName() const {
    if (_rhi_window) {
        return _rhi_window->graphicsApiName();
    }
    return QString();
}

void ViewportWidget::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
}

void ViewportWidget::keyPressEvent(QKeyEvent *event) {
    if (_rhi_window) {
        QCoreApplication::sendEvent(_rhi_window, event);
    }
    QWidget::keyPressEvent(event);
}

void ViewportWidget::keyReleaseEvent(QKeyEvent *event) {
    if (_rhi_window) {
        QCoreApplication::sendEvent(_rhi_window, event);
    }
    QWidget::keyReleaseEvent(event);
}

void ViewportWidget::wheelEvent(QWheelEvent *event) {
    if (_rhi_window) {
        QCoreApplication::sendEvent(_rhi_window, event);
    }
    QWidget::wheelEvent(event);
}

bool ViewportWidget::eventFilter(QObject *obj, QEvent *event) {
    // Intercept RhiWindow mouse events, handle drag logic
    if (obj == _rhi_window && _container) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton &&
                (mouseEvent->modifiers() & Qt::ControlModifier)) {
                _container->setDragStartPos(mouseEvent->pos());
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if ((mouseEvent->buttons() & Qt::LeftButton) &&
                !_container->dragStartPos().isNull()) {
                QPoint delta = mouseEvent->pos() - _container->dragStartPos();
                if (delta.manhattanLength() > 5) {
                    onDragRequested();
                    _container->resetDragStartPos();
                }
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                _container->resetDragStartPos();
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void ViewportWidget::onDragRequested() {
    emit entityDragRequested();
}

}// namespace rbc
