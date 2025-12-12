#pragma once

#include <memory>
#include <luisa/luisa-compute.h>
#include <QtGui/rhi/qrhi.h>

#include "RBCEditorRuntime/config.h"
#include "RBCEditorRuntime/engine/app.h"
#include "RBCEditorRuntime/runtime/Renderer.h"
#include "RBCEditorRuntime/runtime/HttpClient.h"

namespace rbc {

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

    // Accessors
    IApp *getRenderApp() { return m_renderApp.get(); }
    HttpClient *getHttpClient() { return m_httpClient; }
    void setHttpClient(HttpClient *client) { m_httpClient = client; }

    QRhi::Implementation getGraphicsApi() const { return m_graphicsApi; }

private:
    std::unique_ptr<IApp> m_renderApp;

    HttpClient *m_httpClient = nullptr;

    bool m_isInitialized = false;
    bool m_isPaused = false;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;
};

}// namespace rbc
