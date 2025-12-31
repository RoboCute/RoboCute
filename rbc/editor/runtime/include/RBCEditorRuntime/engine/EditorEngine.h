#pragma once

#include <memory>
#include <luisa/luisa-compute.h>
#include <QtGui/rhi/qrhi.h>

#include "RBCEditorRuntime/engine/app_base.h"
#include "RBCEditorRuntime/runtime/Renderer.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"

namespace rbc {

class EditorScene;

class RBC_EDITOR_RUNTIME_API EditorEngine : public IRenderer {
public:
    static EditorEngine &instance();

    EditorEngine();
    ~EditorEngine() override;

    void init(int argc, char **argv);
    void shutdown();

    // IRenderer interface
    void init(QRhiNativeHandles &handles) override;
    void update() override;
    void pause() override;
    void resume() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    void handle_mouse(luisa::compute::MouseButton button, luisa::compute::Action action, luisa::float2 xy) override;
    void handle_cursor_position(luisa::float2 xy) override;

    uint64_t get_present_texture(luisa::uint2 resolution) override;

    // Render Mode Switching
    /**
     * 获取当前渲染模式
     */
    [[nodiscard]] RenderMode getRenderMode() const;

    /**
     * 设置渲染模式
     * 注意：切换渲染模式会重新创建渲染应用，可能会短暂卡顿
     */
    void setRenderMode(RenderMode mode);

    /**
     * 检查渲染模式切换是否可用
     */
    [[nodiscard]] bool canSwitchRenderMode() const { return m_isInitialized && !m_isPaused; }

    // Accessors
    IApp *getRenderApp() { return m_renderApp.get(); }
    AppBase *getRenderAppBase() { return m_renderApp.get(); }
    HttpClient *getHttpClient() { return m_httpClient; }
    void setHttpClient(HttpClient *client) { m_httpClient = client; }
    EditorScene *getEditorScene() { return m_editorScene; }
    void setEditorScene(EditorScene *scene) { m_editorScene = scene; }

    QRhi::Implementation getGraphicsApi() const { return m_graphicsApi; }

private:
    /**
     * 创建指定模式的渲染应用
     */
    std::unique_ptr<AppBase> createRenderApp(RenderMode mode);

private:
    std::unique_ptr<AppBase> m_renderApp;
    RenderMode m_currentRenderMode = RenderMode::Editor;

    // 初始化参数缓存（用于模式切换时重新初始化）
    std::string m_programPath;
    std::string m_backendName;
    luisa::uint2 m_lastResolution{0, 0};

    HttpClient *m_httpClient = nullptr;
    EditorScene *m_editorScene = nullptr;

    bool m_isInitialized = false;
    bool m_isPaused = false;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;
};

}// namespace rbc
