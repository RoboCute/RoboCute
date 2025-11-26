#pragma once
#include <QWindow>
#include <QOffscreenSurface>
#include <rhi/qrhi.h>
#include "RBCEditor/dummyrt.h"
#include <luisa/gui/input.h>

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

struct IRenderer {
    virtual void init(QRhiNativeHandles &) = 0;
    virtual void update() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual void handle_key(luisa::compute::Key key) = 0;
    virtual uint64_t get_present_texture(luisa::uint2 resolution) = 0;
protected:
    ~IRenderer() = default;
};

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
