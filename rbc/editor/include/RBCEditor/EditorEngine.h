#pragma once

#include <memory>
#include <luisa/luisa-compute.h>
#include <QtGui/rhi/qrhi.h>
#include "RBCEditor/dummyapp.h"
#include "RBCEditor/runtime/renderer.h"
#include "RBCEditor/runtime/HttpClient.h"

namespace rbc {

class EditorEngine : public IRenderer {
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
    void handle_key(luisa::compute::Key key) override;
    uint64_t get_present_texture(luisa::uint2 resolution) override;

    // Accessors
    IApp *getRenderApp() { return m_renderApp.get(); }
    HttpClient *getHttpClient() { return m_httpClient; }
    void setHttpClient(HttpClient *client) { m_httpClient = client; }

    QRhi::Implementation getGraphicsApi() const { return m_graphicsApi; }

private:
    std::unique_ptr<luisa::compute::Context> m_context;
    std::unique_ptr<IApp> m_renderApp;

    HttpClient *m_httpClient = nullptr;

    bool m_isPaused = false;
    QRhi::Implementation m_graphicsApi = QRhi::D3D12;
};

}// namespace rbc
