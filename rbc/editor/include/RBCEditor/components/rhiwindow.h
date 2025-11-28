#pragma once
#include <QWindow>
#include <QOffscreenSurface>
#include <rhi/qrhi.h>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWidget>
#include "RBCEditor/runtime/renderer.hpp"
#include <luisa/gui/input.h>

namespace rbc {
inline luisa::compute::Key key_map(int key) {
    using namespace luisa::compute;
    switch (key) {
        case Qt::Key_W: return KEY_W;
        case Qt::Key_A: return KEY_A;
        case Qt::Key_S: return KEY_S;
        case Qt::Key_D: return KEY_D;
        default: return KEY_UNKNOWN;
    }
}

class RhiWindow : public QWindow {
    Q_OBJECT
public:
    RhiWindow(QRhi::Implementation graphicsApi);
    QString graphicsApiName() const;
    void releaseSwapChain();
    std::string workspace_path;
    IRenderer *renderer{};

signals:
    void keyPressed(QString keyInfo);
    void mouseClicked(QString mouseInfo);


protected:
    std::unique_ptr<QRhi> m_rhi;
    std::unique_ptr<QRhiSwapChain> m_sc;
    std::unique_ptr<QRhiRenderBuffer> m_ds;
    std::unique_ptr<QRhiRenderPassDescriptor> m_rp;
    bool m_hasSwapChain = false;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;
    void keyPressEvent(QKeyEvent *event) override;
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

    std::unique_ptr<QRhiBuffer> m_vbuf;
    std::unique_ptr<QRhiBuffer> m_ubuf;
    std::unique_ptr<QRhiTexture> m_texture;
    std::unique_ptr<QRhiSampler> m_sampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_colorTriSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_colorPipeline;
    std::unique_ptr<QRhiShaderResourceBindings> m_fullscreenQuadSrb;
    std::unique_ptr<QRhiGraphicsPipeline> m_fullscreenQuadPipeline;

    QRhiResourceUpdateBatch *m_initialUpdates = nullptr;
};

// 用于包装RhiWindow，转发Event的QWidget
class RHIWindowContainerWidget : public QWidget {
public:
    RhiWindow *rhiWindow = nullptr;

    explicit RHIWindowContainerWidget(RhiWindow *window, QWidget *parent = nullptr)
        : QWidget(parent), rhiWindow(window) {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        qInfo() << "WindowContainerWidget::keyPressEvent - forwarding to RhiWindow";
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
        qInfo() << "WindowContainerWidget::mousePressEvent - setting focus and forwarding";
        setFocus();// 点击时获取焦点
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

}// namespace rbc
