#pragma once
#include <luisa/vstl/common.h>
#include <rbc_graphics/camera.h>
#include <rbc_core/state_map.h>
#include <rbc_runtime/plugin.h>
namespace rbc {
struct RenderPlugin : Plugin {
    struct PipeCtxStub {};
    virtual ~RenderPlugin() = default;
    virtual PipeCtxStub *create_pipeline_context(StateMap &render_settings_map) = 0;
    virtual void destroy_pipeline_context(PipeCtxStub *ctx) = 0;
    virtual Camera &get_camera(PipeCtxStub *pipe_ctx) = 0;

    // render_loop
    virtual bool initialize_pipeline(luisa::string_view pipeline_name) = 0;
    virtual bool before_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) = 0;
    virtual bool on_rendering(luisa::string_view pipeline_name, PipeCtxStub *pipe_ctx) = 0;
};
}// namespace rbc