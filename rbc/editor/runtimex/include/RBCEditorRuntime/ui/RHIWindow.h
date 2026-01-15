#pragma once
#include <rbc_config.h>
#include <QWindow>
#include <QOffscreenSurface>
#include <QtGui/rhi/qrhi.h>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWidget>
#include <QCoreApplication>
#include <luisa/gui/input.h>
#include "RBCEditorRuntime/core/IRenderer.h"

namespace rbc {

// ============================================================================
// Input Mapping Utilities
// ============================================================================

inline luisa::compute::Key key_map(int key) {
    switch (key) {
        case Qt::Key_W: return luisa::compute::KEY_W;
        case Qt::Key_A: return luisa::compute::KEY_A;
        case Qt::Key_S: return luisa::compute::KEY_S;
        case Qt::Key_D: return luisa::compute::KEY_D;
        case Qt::Key_Q: return luisa::compute::KEY_Q;
        case Qt::Key_E: return luisa::compute::KEY_E;
        case Qt::Key_Z: return luisa::compute::KEY_Z;
        case Qt::Key_X: return luisa::compute::KEY_X;
        case Qt::Key_C: return luisa::compute::KEY_C;
        case Qt::Key_V: return luisa::compute::KEY_V;
        case Qt::Key_B: return luisa::compute::KEY_B;
        case Qt::Key_N: return luisa::compute::KEY_N;
        case Qt::Key_M: return luisa::compute::KEY_M;
        case Qt::Key_Comma: return luisa::compute::KEY_COMMA;
        case Qt::Key_Period: return luisa::compute::KEY_PERIOD;
        case Qt::Key_Up: return luisa::compute::KEY_UP;
        case Qt::Key_Down: return luisa::compute::KEY_DOWN;
        case Qt::Key_Left: return luisa::compute::KEY_LEFT;
        case Qt::Key_Right: return luisa::compute::KEY_RIGHT;
        case Qt::Key_Space: return luisa::compute::KEY_SPACE;
        case Qt::Key_Enter: return luisa::compute::KEY_ENTER;
        case Qt::Key_Escape: return luisa::compute::KEY_ESCAPE;
        case Qt::Key_Tab: return luisa::compute::KEY_TAB;
        case Qt::Key_Backspace: return luisa::compute::KEY_BACKSPACE;
        case Qt::Key_Delete: return luisa::compute::KEY_DELETE;
        case Qt::Key_Home: return luisa::compute::KEY_HOME;
        case Qt::Key_End: return luisa::compute::KEY_END;
        case Qt::Key_PageUp: return luisa::compute::KEY_PAGE_UP;
        case Qt::Key_PageDown: return luisa::compute::KEY_PAGE_DOWN;
        case Qt::Key_Insert: return luisa::compute::KEY_INSERT;
        case Qt::Key_Control: return luisa::compute::KEY_LEFT_CONTROL;
        case Qt::Key_Shift: return luisa::compute::KEY_LEFT_SHIFT;
        case Qt::Key_Alt: return luisa::compute::KEY_LEFT_ALT;
        case Qt::Key_F1: return luisa::compute::KEY_F1;
        case Qt::Key_F2: return luisa::compute::KEY_F2;
        case Qt::Key_F3: return luisa::compute::KEY_F3;
        case Qt::Key_F4: return luisa::compute::KEY_F4;
        case Qt::Key_F5: return luisa::compute::KEY_F5;
        case Qt::Key_F6: return luisa::compute::KEY_F6;
        case Qt::Key_F7: return luisa::compute::KEY_F7;
        case Qt::Key_F8: return luisa::compute::KEY_F8;
        case Qt::Key_F9: return luisa::compute::KEY_F9;
        case Qt::Key_F10: return luisa::compute::KEY_F10;
        case Qt::Key_F11: return luisa::compute::KEY_F11;
        case Qt::Key_F12: return luisa::compute::KEY_F12;
        default: return luisa::compute::KEY_UNKNOWN;
    }
}

inline luisa::compute::Action action_map(int action) {
    using namespace luisa::compute;
    switch (action) {
        case QEvent::KeyPress: return Action::ACTION_PRESSED;
        case QEvent::KeyRelease: return Action::ACTION_RELEASED;
        case QEvent::MouseButtonPress: return Action::ACTION_PRESSED;
        case QEvent::MouseButtonRelease: return Action::ACTION_RELEASED;
        default: return Action::ACTION_UNKNOWN;
    }
}

inline luisa::compute::MouseButton mouse_button_map(int mouse_button) {
    using namespace luisa::compute;
    switch (mouse_button) {
        case Qt::LeftButton: return MOUSE_BUTTON_LEFT;
        case Qt::RightButton: return MOUSE_BUTTON_RIGHT;
        case Qt::MiddleButton: return MOUSE_BUTTON_MIDDLE;
        default: return MOUSE_BUTTON_UNKNOWN;
    }
}

inline luisa::float2 mouse_pos_map(float x, float y) {
    return {x, y};
}

// ============================================================================
// RhiWindow - Qt RHI Rendering Window
// ============================================================================

/**
 * @brief RhiWindow - 基于 Qt RHI 的渲染窗口
 * 
 * 这是一个 QWindow 子类，负责：
 * 1. 初始化 RHI 后端（D3D12/Vulkan/Metal）
 * 2. 管理 SwapChain 和渲染管线
 * 3. 将渲染器输出的纹理渲染到窗口
 * 4. 处理输入事件并转发给渲染器
 */
class RBC_EDITOR_RUNTIME_API RhiWindow : public QWindow {
    Q_OBJECT

public:
    explicit RhiWindow(QRhi::Implementation graphicsApi);
    ~RhiWindow() override = default;

    [[nodiscard]] QString graphicsApiName() const;
    void releaseSwapChain();
    
    // Renderer 由外部设置，RhiWindow 不拥有其生命周期
    IRenderer *renderer = nullptr;
    
    // Workspace path (可选，用于资源加载)
    std::string workspace_path;

signals:
    void keyPressed(QString keyInfo);
    void mouseClicked(QString mouseInfo);

protected:
    // RHI Resources
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QRhiSwapChain> m_sc;
    std::unique_ptr<QRhiRenderBuffer> m_ds;
    std::unique_ptr<QRhiRenderPassDescriptor> m_rp;
    bool m_hasSwapChain = false;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;

    // Input event handlers
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void init();
    void resizeSwapChain();
    void render();

    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *) override;
    void ensureFullscreenTexture(const QSize &pixelSize, QRhiResourceUpdateBatch *u);

    bool m_initialized = false;
    bool m_notExposed = false;
    bool m_newlyExposed = false;

    // Fullscreen quad resources
    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_fullscreenQuadSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_fullscreenQuadPipeline;

    QRhiResourceUpdateBatch *m_initialUpdates = nullptr;
};

// ============================================================================
// RHIWindowContainerWidget - Event Forwarding Container
// ============================================================================

/**
 * @brief RHIWindowContainerWidget - 用于转发事件的 QWidget 容器
 * 
 * 这个 widget 包装了 RhiWindow，负责：
 * 1. 将键盘/鼠标事件转发给 RhiWindow
 * 2. 处理焦点管理
 */
class RBC_EDITOR_RUNTIME_API RHIWindowContainerWidget : public QWidget {
    Q_OBJECT

public:
    RhiWindow *rhiWindow = nullptr;

    explicit RHIWindowContainerWidget(RhiWindow *window, QWidget *parent = nullptr)
        : QWidget(parent), rhiWindow(window) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (rhiWindow) {
            QCoreApplication::sendEvent(rhiWindow, event);
        }
        QWidget::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent *event) override {
        if (rhiWindow) {
            QCoreApplication::sendEvent(rhiWindow, event);
        }
        QWidget::keyReleaseEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override {
        setFocus(); // 点击时获取焦点
        if (rhiWindow) {
            QCoreApplication::sendEvent(rhiWindow, event);
        }
        QWidget::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (rhiWindow) {
            QCoreApplication::sendEvent(rhiWindow, event);
        }
        QWidget::mouseReleaseEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (rhiWindow) {
            QCoreApplication::sendEvent(rhiWindow, event);
        }
        QWidget::mouseMoveEvent(event);
    }

    void wheelEvent(QWheelEvent *event) override {
        if (rhiWindow) {
            QCoreApplication::sendEvent(rhiWindow, event);
        }
        QWidget::wheelEvent(event);
    }
};

} // namespace rbc
