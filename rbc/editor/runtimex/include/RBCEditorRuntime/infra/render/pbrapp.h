#pragma once
#include "RBCEditorRuntime/infra/render/app_base.h"
// #include "RBCEditorRuntime/runtime/RenderScene.h"

namespace rbc {

/**
 * PBRApp - 真实感渲染应用
 * 
 * 用于预览真实感渲染效果：
 * - 路径追踪渲染
 * - PBR材质渲染
 * - 光照效果预览
 */
struct PBRApp : public RenderAppBase {
    // vstd::optional<rbc::SimpleScene> simple_scene;
    vstd::optional<float3> cube_move, light_move;
    RenderPlugin::PipeCtxStub* pipe_ctx{};

public:
    [[nodiscard]] RenderMode getRenderMode() const override { return RenderMode::PBR; }

    void update() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;

    ~PBRApp() override;

protected:
    /**
     * PBRApp 特有的初始化
     */
    void on_init() override;
};

}// namespace rbc
