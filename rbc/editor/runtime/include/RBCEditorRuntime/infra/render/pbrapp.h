#pragma once
#include "RBCEditorRuntime/infra/render/app_base.h"
// #include "RBCEditorRuntime/runtime/RenderScene.h"

namespace rbc {

/**
 * PBRApp - Photorealistic rendering application
 * 
 * Used for previewing photorealistic rendering effects:
 * - Path tracing rendering
 * - PBR material rendering
 * - Lighting effect preview
 */
struct PBRApp : public RenderAppBase {
    // vstd::optional<rbc::SimpleScene> simple_scene;
    vstd::optional<float3> cube_move, light_move;

public:
    [[nodiscard]] RenderMode getRenderMode() const override { return RenderMode::PBR; }

    void update() override;
    void handle_key(luisa::compute::Key key, luisa::compute::Action action) override;
    ~PBRApp() override;

protected:
    /**
     * PBRApp-specific initialization
     */
    void on_init() override;
};

}// namespace rbc
