#include "RBCEditorRuntime/ui/RHIWindow.h"
#include <QtGui/rhi/qrhi_platform.h>
#include <QPlatformSurfaceEvent>
#include <QPainter>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QtGui/rhi/qshader.h>
#include <luisa/vstl/common.h>
#include <luisa/runtime/context.h>

namespace rbc {

static QShader getShader(const QString &name) {
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());
    return QShader();
}

RhiWindow::RhiWindow(QRhi::Implementation graphicsApi)
    : _graphics_api(graphicsApi) {
    switch (graphicsApi) {
        case QRhi::D3D12:
            setSurfaceType(Direct3DSurface);
            break;
        case QRhi::Vulkan:
            setSurfaceType(VulkanSurface);
            break;
        case QRhi::Metal:
            setSurfaceType(MetalSurface);
            break;
        default:
            setSurfaceType(Direct3DSurface);
            break;
    }
    // Set window flags to receive keyboard events
    setFlags(Qt::Window | Qt::FramelessWindowHint);
}

QString RhiWindow::graphicsApiName() const {
    switch (_graphics_api) {
        case QRhi::D3D12:
            return QLatin1String("Direct3D 12");
        case QRhi::Vulkan:
            return QLatin1String("Vulkan Backend");
        case QRhi::Metal:
            return QLatin1String("Metal Backend");
        default:
            return QLatin1String("Unknown Backend");
    }
}

void RhiWindow::exposeEvent(QExposeEvent *) {
    // Triggered when window is exposed
    if (isExposed() && !_initialized) {
        init();
        resizeSwapChain();
        _initialized = true;
    }

    const QSize surfaceSize = _has_swap_chain ? _sc->surfacePixelSize() : QSize();

    // Stop rendering when window is not visible or size is zero
    if ((!isExposed() || (_has_swap_chain && surfaceSize.isEmpty())) && _initialized && !_not_exposed)
        _not_exposed = true;

    // Resume rendering when window is visible again and size is valid
    if (isExposed() && _initialized && _not_exposed && !surfaceSize.isEmpty()) {
        _not_exposed = false;
        _newly_exposed = true;
    }

    // Render a frame during exposeEvent to immediately respond to window resize
    if (isExposed() && !surfaceSize.isEmpty())
        render();
}

bool RhiWindow::event(QEvent *e) {
    switch (e->type()) {
        case QEvent::UpdateRequest:
            render();
            break;

        case QEvent::PlatformSurface:
            // Release swapchain while native window and surface still exist
            if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() ==
                QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                releaseSwapChain();
            break;

        default:
            break;
    }

    return QWindow::event(e);
}

void RhiWindow::init() {
    LUISA_ASSERT(renderer, "Renderer must be set before initialization.");
    if (_graphics_api == QRhi::D3D12) {
        QRhiD3D12NativeHandles handles;
        QRhiD3D12InitParams params;
#ifdef DEBUG
        params.enableDebugLayer = true;
#else
        params.enableDebugLayer = false;
#endif
        renderer->process_qt_handle(handles);
        _rhi.reset(QRhi::create(QRhi::D3D12, &params, {}, &handles));
    }
    // else if (_graphics_api == QRhi::Vulkan) {
    //     QRhiVulkanInitParams params;
    //     QRhiVulkanNativeHandles handles;
    //     params.inst = vulkanInstance();
    //     params.window = this;
    //     renderer->init(handles);
    //     handles.inst = vulkanInstance();
    //     _rhi.reset(QRhi::create(QRhi::Vulkan, &params, {}, &handles));
    // }

    if (!_rhi)
        qFatal("Failed to create RHI backend");

    _sc.reset(_rhi->newSwapChain());
    _ds.reset(_rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil,
        QSize(),// UsedWithSwapChainOnly no need to specify size
        1, QRhiRenderBuffer::UsedWithSwapChainOnly));

    _sc->setWindow(this);
    _sc->setDepthStencil(_ds.get());
    _rp.reset(_sc->newCompatibleRenderPassDescriptor());
    _sc->setRenderPassDescriptor(_rp.get());

    _initial_updates = _rhi->nextResourceUpdateBatch();

    ensureFullscreenTexture(_sc->surfacePixelSize(), _initial_updates);

    _sampler.reset(_rhi->newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    _sampler->create();

    _fullscreen_quad_srb.reset(_rhi->newShaderResourceBindings());
    _fullscreen_quad_srb->setBindings({QRhiShaderResourceBinding::sampledTexture(
        0, QRhiShaderResourceBinding::FragmentStage,
        _texture.get(), _sampler.get())});
    _fullscreen_quad_srb->create();

    _fullscreen_quad_pipeline.reset(_rhi->newGraphicsPipeline());
    _fullscreen_quad_pipeline->setShaderStages(
        {{QRhiShaderStage::Vertex,
          getShader(QLatin1String(":/quad.vert.qsb"))},
         {QRhiShaderStage::Fragment,
          getShader(QLatin1String(":/quad.frag.qsb"))}});
    _fullscreen_quad_pipeline->setVertexInputLayout({});
    _fullscreen_quad_pipeline->setShaderResourceBindings(_fullscreen_quad_srb.get());
    _fullscreen_quad_pipeline->setRenderPassDescriptor(_rp.get());
    _fullscreen_quad_pipeline->create();
}

void RhiWindow::resizeSwapChain() {
    _has_swap_chain = _sc->createOrResize();
}

void RhiWindow::releaseSwapChain() {
    if (_has_swap_chain) {
        _has_swap_chain = false;
        _sc->destroy();
    }
}

void RhiWindow::render() {
    if (!_has_swap_chain || _not_exposed)
        return;

    if (_sc->currentPixelSize() != _sc->surfacePixelSize() || _newly_exposed) {
        resizeSwapChain();
        if (!_has_swap_chain)
            return;
        _newly_exposed = false;
    }

    QRhi::FrameOpResult result = _rhi->beginFrame(_sc.get());

    if (result == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!_has_swap_chain)
            return;
        result = _rhi->beginFrame(_sc.get());
    }

    if (result != QRhi::FrameOpSuccess) {
        qWarning("beginFrame failed with %d, will retry", result);
        requestUpdate();
        return;
    }

    QRhiResourceUpdateBatch *resourceUpdates = _rhi->nextResourceUpdateBatch();

    if (_initial_updates) {
        resourceUpdates->merge(_initial_updates);
        _initial_updates->release();
        _initial_updates = nullptr;
    }

    QRhiCommandBuffer *cb = _sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = _sc->currentPixelSize();
    ensureFullscreenTexture(outputSizeInPixels, resourceUpdates);

    // Update renderer
    renderer->update();

    cb->beginPass(_sc->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, resourceUpdates);
    cb->setGraphicsPipeline(_fullscreen_quad_pipeline.get());
    cb->setViewport({0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height())});
    cb->setShaderResources();
    cb->draw(3);
    cb->endPass();

    _rhi->endFrame(_sc.get());
    requestUpdate();
}

void RhiWindow::ensureFullscreenTexture(const QSize &pixelSize, QRhiResourceUpdateBatch *u) {
    if (_texture && _texture->pixelSize() == pixelSize)
        return;

    if (!_texture)
        _texture.reset(_rhi->newTexture(QRhiTexture::RGBA8, pixelSize));
    else
        _texture->setPixelSize(pixelSize);

    uint64_t handle = renderer->get_present_texture(
        pixelSize.width(), pixelSize.height());
    // D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 128
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL = 5
    _texture->createFrom({handle, _graphics_api == QRhi::Vulkan ? 5 : 128});
}

void RhiWindow::keyPressEvent(QKeyEvent *event) {
    // Forward to renderer
    renderer->handle_key(key_map(event->key()), action_map(event->type()));

    // Emit signal for debugging
    QString keyName;
    switch (event->key()) {
        case Qt::Key_W: keyName = "W"; break;
        case Qt::Key_A: keyName = "A"; break;
        case Qt::Key_S: keyName = "S"; break;
        case Qt::Key_D: keyName = "D"; break;
        default: keyName = QString("Key(%1)").arg(event->key()); break;
    }

    QString keyInfo = QString("Key Pressed: %1 (code: %2)").arg(keyName).arg(event->key());
    emit keyPressed(keyInfo);

    QWindow::keyPressEvent(event);
}

void RhiWindow::keyReleaseEvent(QKeyEvent *event) {
    renderer->handle_key(key_map(event->key()), action_map(event->type()));
    QWindow::keyReleaseEvent(event);
}

void RhiWindow::mousePressEvent(QMouseEvent *event) {
    // Convert logical coordinates to physical pixel coordinates (supports high DPI)
    qreal dpr = devicePixelRatio();
    float physicalX = static_cast<float>(event->pos().x() * dpr);
    float physicalY = static_cast<float>(event->pos().y() * dpr);

    renderer->handle_mouse(
        mouse_button_map(event->button()),
        action_map(event->type()),
        mouse_pos_map(physicalX, physicalY));

    QString buttonName;
    switch (event->button()) {
        case Qt::LeftButton: buttonName = "Left"; break;
        case Qt::RightButton: buttonName = "Right"; break;
        case Qt::MiddleButton: buttonName = "Middle"; break;
        default: buttonName = "Unknown"; break;
    }

    QString mouseInfo = QString("Mouse Clicked: %1 button at (%2, %3)")
                            .arg(buttonName)
                            .arg(event->pos().x())
                            .arg(event->pos().y());
    emit mouseClicked(mouseInfo);

    QWindow::mousePressEvent(event);
}

void RhiWindow::mouseReleaseEvent(QMouseEvent *event) {
    qreal dpr = devicePixelRatio();
    float physicalX = static_cast<float>(event->pos().x() * dpr);
    float physicalY = static_cast<float>(event->pos().y() * dpr);

    renderer->handle_mouse(
        mouse_button_map(event->button()),
        action_map(event->type()),
        mouse_pos_map(physicalX, physicalY));

    QWindow::mouseReleaseEvent(event);
}

void RhiWindow::mouseMoveEvent(QMouseEvent *event) {
    qreal dpr = devicePixelRatio();
    float physicalX = static_cast<float>(event->pos().x() * dpr);
    float physicalY = static_cast<float>(event->pos().y() * dpr);

    renderer->handle_cursor_position(mouse_pos_map(physicalX, physicalY));

    QWindow::mouseMoveEvent(event);
}

}// namespace rbc
