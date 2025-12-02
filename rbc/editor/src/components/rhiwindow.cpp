#include "RBCEditor/components/RHIWindow.h"
#include "qbytearrayview.h"
#include "rhi/qrhi_platform.h"
#include <QPlatformSurfaceEvent>
#include <QPainter>
#include <QFile>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QDebug>
#include <rhi/qshader.h>

namespace rbc {
static QShader getShader(const QString &name) {
    QFile f(name);
    if (f.open(QIODevice::ReadOnly))
        return QShader::fromSerialized(f.readAll());

    return QShader();
}

RhiWindow::RhiWindow(QRhi::Implementation graphicsApi)
    : m_graphicsApi(graphicsApi) {
    // setSurfaceType(Direct3DSurface);
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
    // 设置窗口标志以接收键盘事件
    setFlags(Qt::Window | Qt::FramelessWindowHint);
}

QString RhiWindow::graphicsApiName() const {
    switch (m_graphicsApi) {
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
    /// 属于Window的特殊事件，将Window展示出来的时候触发exposeEvent
    // initialize and start rendering when the window becomes usable for graphics purposes
    if (isExposed() && !m_initialized) {
        init();
        resizeSwapChain();
        m_initialized = true;
    }

    const QSize surfaceSize = m_hasSwapChain ? m_sc->surfacePixelSize() : QSize();

    // stop pushing frames when not exposed (or size is 0)
    if ((!isExposed() || (m_hasSwapChain && surfaceSize.isEmpty())) && m_initialized && !m_notExposed)
        m_notExposed = true;

    // Continue when exposed again and the surface has a valid size. Note that
    // surfaceSize can be (0, 0) even though size() reports a valid one, hence
    // trusting surfacePixelSize() and not QWindow.
    if (isExposed() && m_initialized && m_notExposed && !surfaceSize.isEmpty()) {
        m_notExposed = false;
        m_newlyExposed = true;
    }

    // always render a frame on exposeEvent() (when exposed) in order to update
    // immediately on window resize.
    if (isExposed() && !surfaceSize.isEmpty())
        render();
}

bool RhiWindow::event(QEvent *e) {
    // 通用事件处理
    switch (e->type()) {
        case QEvent::UpdateRequest:
            render();
            break;

        case QEvent::PlatformSurface:
            // this is the proper time to tear down the swapchain (while the native window and surface are still around)
            if (static_cast<QPlatformSurfaceEvent *>(e)->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
                releaseSwapChain();
            break;

        default:
            break;
    }

    return QWindow::event(e);
}

void RhiWindow::init() {
    LUISA_ASSERT(renderer, "Initialize callback not setted.");
    if (m_graphicsApi == QRhi::D3D12) {
        QRhiD3D12NativeHandles handles;
        QRhiD3D12InitParams params;
#ifdef DEBUG
        params.enableDebugLayer = true;
#else
        params.enableDebugLayer = false;
#endif
        renderer->init(handles);
        m_rhi.reset(QRhi::create(QRhi::D3D12, &params, {}, &handles));
    } else if (m_graphicsApi == QRhi::Vulkan) {
        QRhiVulkanInitParams params;
        QRhiVulkanNativeHandles handles;
        params.inst = vulkanInstance();
        params.window = this;
        renderer->init(handles);
        handles.inst = vulkanInstance();
        m_rhi.reset(QRhi::create(QRhi::Vulkan, &params, {}, &handles));
    }

    if (!m_rhi)
        qFatal("Failed to create RHI backend");

    m_sc.reset(m_rhi->newSwapChain());
    m_ds.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil,
                                      QSize(),// no need to set the size here, due to UsedWithSwapChainOnly
                                      1, QRhiRenderBuffer::UsedWithSwapChainOnly));
    m_sc->setWindow(this);
    m_sc->setDepthStencil(m_ds.get());
    m_rp.reset(m_sc->newCompatibleRenderPassDescriptor());
    m_sc->setRenderPassDescriptor(m_rp.get());

    m_initialUpdates = m_rhi->nextResourceUpdateBatch();

    ensureFullscreenTexture(m_sc->surfacePixelSize(), m_initialUpdates);

    m_sampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_sampler->create();

    m_fullscreenQuadSrb.reset(m_rhi->newShaderResourceBindings());
    m_fullscreenQuadSrb->setBindings({QRhiShaderResourceBinding::sampledTexture(0, QRhiShaderResourceBinding::FragmentStage, m_texture.get(), m_sampler.get())});
    m_fullscreenQuadSrb->create();

    m_fullscreenQuadPipeline.reset(m_rhi->newGraphicsPipeline());
    m_fullscreenQuadPipeline->setShaderStages({{QRhiShaderStage::Vertex, getShader(QLatin1String(":/quad.vert.qsb"))}, {QRhiShaderStage::Fragment, getShader(QLatin1String(":/quad.frag.qsb"))}});
    m_fullscreenQuadPipeline->setVertexInputLayout({});
    m_fullscreenQuadPipeline->setShaderResourceBindings(m_fullscreenQuadSrb.get());
    m_fullscreenQuadPipeline->setRenderPassDescriptor(m_rp.get());
    m_fullscreenQuadPipeline->create();
}

void RhiWindow::resizeSwapChain() {
    m_hasSwapChain = m_sc->createOrResize();// also handles m_ds
}
void RhiWindow::releaseSwapChain() {
    if (m_hasSwapChain) {
        m_hasSwapChain = false;
        m_sc->destroy();
    }
}
void RhiWindow::render() {
    if (!m_hasSwapChain || m_notExposed)
        return;
    if (m_sc->currentPixelSize() != m_sc->surfacePixelSize() || m_newlyExposed) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        m_newlyExposed = false;
    }

    QRhi::FrameOpResult result = m_rhi->beginFrame(m_sc.get());

    if (result == QRhi::FrameOpSwapChainOutOfDate) {
        resizeSwapChain();
        if (!m_hasSwapChain)
            return;
        result = m_rhi->beginFrame(m_sc.get());
    }
    if (result != QRhi::FrameOpSuccess) {
        qWarning("beginFrame failed with %d, will retry", result);
        requestUpdate();
        return;
    }

    QRhiResourceUpdateBatch *resourceUpdates = m_rhi->nextResourceUpdateBatch();

    if (m_initialUpdates) {
        resourceUpdates->merge(m_initialUpdates);
        m_initialUpdates->release();
        m_initialUpdates = nullptr;
    }

    QRhiCommandBuffer *cb = m_sc->currentFrameCommandBuffer();
    const QSize outputSizeInPixels = m_sc->currentPixelSize();
    ensureFullscreenTexture(outputSizeInPixels, resourceUpdates);

    renderer->update();

    cb->beginPass(m_sc->currentFrameRenderTarget(), Qt::black, {1.0f, 0}, resourceUpdates);
    cb->setGraphicsPipeline(m_fullscreenQuadPipeline.get());
    cb->setViewport({0, 0, float(outputSizeInPixels.width()), float(outputSizeInPixels.height())});
    cb->setShaderResources();
    cb->draw(3);
    cb->endPass();

    m_rhi->endFrame(m_sc.get());
    requestUpdate();
}

void RhiWindow::ensureFullscreenTexture(const QSize &pixelSize, QRhiResourceUpdateBatch *u) {
    if (m_texture && m_texture->pixelSize() == pixelSize)
        return;

    if (!m_texture)
        m_texture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, pixelSize));
    else
        m_texture->setPixelSize(pixelSize);

    uint64_t handle = renderer->get_present_texture(luisa::uint2(pixelSize.width(), pixelSize.height()));
    // D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    m_texture->createFrom({handle, m_graphicsApi == QRhi::Vulkan ? 5 : 128});
}

void RhiWindow::keyPressEvent(QKeyEvent *event) {
    QString keyName;
    // Handle Key Event
    renderer->handle_key(key_map(event->key()));

    // Debug Info handler
    switch (event->key()) {
        case Qt::Key_W: {
            keyName = "W";
            break;
        }
        case Qt::Key_A: {
            keyName = "A";
            break;
        }
        case Qt::Key_S: {
            keyName = "S";
            break;
        }
        case Qt::Key_D: {
            keyName = "D";
            break;
        }
        default: keyName = QString("Key(%1)").arg(event->key()); break;
    }

    QString keyInfo = QString("Key Pressed: %1 (code: %2)").arg(keyName).arg(event->key());
    qInfo() << keyInfo;
    emit keyPressed(keyInfo);

    // Root Window Handler
    QWindow::keyPressEvent(event);
}

void RhiWindow::mousePressEvent(QMouseEvent *event) {
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
    qInfo() << mouseInfo;
    emit mouseClicked(mouseInfo);

    QWindow::mousePressEvent(event);
}

void RhiWindow::mouseReleaseEvent(QMouseEvent *event) {
    qInfo() << "RhiWindow::mouseReleaseEvent - Button:" << event->button();
    QWindow::mouseReleaseEvent(event);
}

void RhiWindow::mouseMoveEvent(QMouseEvent *event) {
    // 鼠标移动事件会很频繁，可以选择性输出
    // qDebug() << "RhiWindow::mouseMoveEvent - Pos:" << event->pos();
    QWindow::mouseMoveEvent(event);
}
}// namespace rbc
