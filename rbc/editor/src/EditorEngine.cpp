#include "RBCEditor/EditorEngine.h"
#include "QtGui/rhi/qrhi_platform.h"
#include <QDebug>
#include "RBCEditor/dummyapp.h"
#include "RBCEditor/naiveapp.h"
#include "RBCEditor/pbrapp.h"

namespace rbc {

EditorEngine &EditorEngine::instance() {
    static EditorEngine s_instance;
    return s_instance;
}

EditorEngine::EditorEngine() = default;

EditorEngine::~EditorEngine() {
    shutdown();
}

void EditorEngine::init(int argc, char **argv) {
    if (m_context) return;
    // Initialize LuisaCompute Context
    m_context = std::make_unique<luisa::compute::Context>(argv[0]);
    // Initialize Renderer App
    luisa::string backend = "dx";
    m_graphicsApi = QRhi::D3D12;

    // m_renderApp = std::make_unique<DummyApp>();
    // m_renderApp = std::make_unique<NaiveApp>();
    m_renderApp = std::make_unique<PBRApp>();

    m_renderApp->init(*m_context, backend.c_str());
}

void EditorEngine::shutdown() {
    m_renderApp.reset();
    m_context.reset();
}

void EditorEngine::init(QRhiNativeHandles &handles_base) {
    if (!m_renderApp) return;

    if (m_graphicsApi == QRhi::D3D12) {
        // bind Renderer Handle to Qt Handle
        // Native DX Device
        // Native DX Adapter
        // Native DX Command Queue
        auto &handles = static_cast<QRhiD3D12NativeHandles &>(handles_base);
        handles.dev = m_renderApp->GetDeviceNativeHandle();
        handles.minimumFeatureLevel = 0;
        handles.adapterLuidHigh = (qint32)m_renderApp->GetDXAdapterLUIDHigh();
        handles.adapterLuidLow = (qint32)m_renderApp->GetDXAdapterLUIDLow();
        handles.commandQueue = m_renderApp->GetStreamNativeHandle();
    } else {
        LUISA_ERROR("Backend unsupported.");
    }
}

void EditorEngine::update() {
    if (m_isPaused || !m_renderApp) return;
    m_renderApp->update();
}

void EditorEngine::pause() {
    m_isPaused = true;
}

void EditorEngine::resume() {
    m_isPaused = false;
}

void EditorEngine::handle_key(luisa::compute::Key key) {
    if (m_renderApp) {
        m_renderApp->handle_key(key);
    }
}

uint64_t EditorEngine::get_present_texture(luisa::uint2 resolution) {
    if (m_renderApp) {
        return m_renderApp->create_texture(resolution.x, resolution.y);
    }
    return 0;
}

}// namespace rbc
