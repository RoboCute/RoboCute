#pragma once
#include <rbc_config.h>
#include <QWidget>
#include "RBCEditorRuntime/ui/RHIWindow.h"
#include "RBCEditorRuntime/infra/render/app_base.h"

namespace rbc {

class ViewportWidget;

// ============================================================================
// ViewportContainerWidget - Mouse Event Handler and Drag Support
// ============================================================================

/**
 * @brief ViewportContainerWidget - Custom container for mouse events and drag support
 *
 * This widget wraps the RhiWindow container, responsible for:
 * 1. Detecting drag start position
 * 2. Emitting drag request signals
 * 3. Forwarding events to RhiWindow
 */
class RBC_EDITOR_RUNTIME_API ViewportContainerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ViewportContainerWidget(RhiWindow *rhiWindow, QWidget *parent = nullptr);

    void setDragStartPos(const QPoint &pos) { _drag_start_pos = pos; }
    QPoint dragStartPos() const { return _drag_start_pos; }
    void resetDragStartPos() { _drag_start_pos = QPoint(); }

    QWidget *innerContainer() const { return _inner_container; }

signals:
    void dragRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    RhiWindow *_rhi_window = nullptr;
    QWidget *_inner_container = nullptr;
    QPoint _drag_start_pos;
};

// ============================================================================
// ViewportWidget - Main Viewport Component
// ============================================================================

/**
 * @brief ViewportWidget - Viewport component
 *
 * The Viewport component is a QWidget wrapper for the rendering window, responsible for:
 * 1. Creating and managing RhiWindow
 * 2. Forwarding window messages to the renderer
 * 3. Supporting entity drag-and-drop operations
 *
 * Usage:
 * @code
 * IRenderer* renderer = createRenderer();
 * ViewportWidget* viewport = new ViewportWidget(renderer, parent);
 * viewport manages the lifecycle of RhiWindow
 * @endcode
 */
class RBC_EDITOR_RUNTIME_API ViewportWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param renderer Renderer interface, ViewportWidget does not own its lifecycle
     * @param graphicsApi RHI graphics API type
     * @param parent Parent widget
     */
    explicit ViewportWidget(IRenderer *renderer,
                            QRhi::Implementation graphicsApi = QRhi::D3D12,
                            QWidget *parent = nullptr);
    ~ViewportWidget() override;

    [[nodiscard]] RhiWindow *getRhiWindow() const { return _rhi_window; }

    /**
     * @brief Get the graphics API name
     */
    [[nodiscard]] QString graphicsApiName() const;

signals:
    /**
     * @brief Drag request signal
     *
     * Emitted when the user drags a selected entity in the viewport
     */
    void entityDragRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onDragRequested();

private:
    void setupUi();

    IRenderer *_renderer = nullptr;
    RhiWindow *_rhi_window = nullptr;
    ViewportContainerWidget *_container = nullptr;
    QRhi::Implementation _graphics_api = QRhi::D3D12;
};

}// namespace rbc
