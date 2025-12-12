#include "RBCEditorRuntime/components/ViewportWidget.h"
#include "RBCEditorRuntime/EditorEngine.h"
#include <QVBoxLayout>
#include <QCoreApplication>
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

}// namespace rbc
