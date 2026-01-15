#include "RBCEditorRuntime/engine/EditorEngine.h"
#include "RBCEditorRuntime/runtime/EditorScene.h"
#include "QtGui/rhi/qrhi_platform.h"
#include <QDebug>
#include <luisa/core/logging.h>

#include "RBCEditorRuntime/engine/pbrapp.h"
#include "RBCEditorRuntime/engine/visapp.h"

#include <rbc_core/runtime_static.h>

namespace rbc {

EditorEngine &EditorEngine::instance() {
    static EditorEngine s_instance;
    return s_instance;
}

EditorEngine::EditorEngine() = default;

EditorEngine::~EditorEngine() {
    shutdown();
}

std::unique_ptr<AppBase> EditorEngine::createRenderApp(RenderMode mode) {
    switch (mode) {
        case RenderMode::Editor:
            return std::make_unique<VisApp>();
        case RenderMode::Realistic:
            return std::make_unique<PBRApp>();
        default:
            return std::make_unique<VisApp>();
    }
}

void EditorEngine::init(int argc, char **argv) {
    if (m_isInitialized) return;

    m_backendName = "dx";
    m_programPath = argv[0];
    m_graphicsApi = QRhi::D3D12;

    // 默认使用编辑器模式
    m_currentRenderMode = RenderMode::Editor;
    m_renderApp = createRenderApp(m_currentRenderMode);
    m_renderApp->init(m_programPath.c_str(), m_backendName.c_str());

    m_isInitialized = true;
}

void EditorEngine::shutdown() {
    m_renderApp.reset();
    m_isInitialized = false;
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

    // Process pending render operations before the frame
    // This must happen after the previous frame's IO is complete
    if (m_editorScene) {
        m_editorScene->onFrameTick();
    }

    m_renderApp->update();
}

void EditorEngine::pause() {
    m_isPaused = true;
}

void EditorEngine::resume() {
    m_isPaused = false;
}

void EditorEngine::handle_key(luisa::compute::Key key, luisa::compute::Action action) {
    if (m_renderApp) {
        m_renderApp->handle_key(key, action);
    }
}

void EditorEngine::handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) {
    if (m_renderApp) {
        m_renderApp->handle_mouse(button, action, xy);
    }
}

void EditorEngine::handle_cursor_position(luisa::float2 xy) {
    if (m_renderApp) {
        m_renderApp->handle_cursor_position(xy);
    }
}

uint64_t EditorEngine::get_present_texture(luisa::uint2 resolution) {
    if (m_renderApp) {
        m_lastResolution = resolution;
        return m_renderApp->create_texture(resolution.x, resolution.y);
    }
    return 0;
}

RenderMode EditorEngine::getRenderMode() const {
    if (m_renderApp) {
        return m_renderApp->getRenderMode();
    }
    return m_currentRenderMode;
}

void EditorEngine::setRenderMode(RenderMode mode) {
    if (!canSwitchRenderMode()) {
        LUISA_WARNING("Cannot switch render mode: engine not ready");
        return;
    }

    if (mode == m_currentRenderMode) {
        return;
    }

    LUISA_INFO("Switching render mode from {} to {}",
               m_currentRenderMode == RenderMode::Editor ? "Editor" : "Realistic",
               mode == RenderMode::Editor ? "Editor" : "Realistic");

    // 注意：由于GraphicsUtils和RenderDevice是全局单例，
    // 我们不能简单地销毁和重建渲染应用。
    // 当前实现仅记录模式切换，实际的完整切换需要更复杂的状态管理。
    //
    // TODO: 完整的渲染模式切换实现需要：
    // 1. 保存当前相机状态
    // 2. 同步销毁旧应用
    // 3. 创建新应用
    // 4. 恢复相机状态
    // 5. 重新创建纹理
    //
    // 当前简化实现：仅在启动时切换模式有效
    m_currentRenderMode = mode;

    LUISA_WARNING("Runtime render mode switching is not fully implemented yet. "
                  "Please restart the editor with the desired mode.");
}

}// namespace rbc
