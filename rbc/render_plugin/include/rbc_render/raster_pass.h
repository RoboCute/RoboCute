#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/raster/depth_buffer.h>
#include <luisa/core/fiber.h>
#include <rbc_render/pipeline_context.h>
#include <rbc_render/click_manager.h>
#include <raster/raster_args.hpp>
#include <rbc_graphics/scene_manager.h>
namespace rbc {

struct RasterPass : public Pass {// struct Reservoir
private:
    luisa::fiber::counter _init_counter;
    RasterShader<Buffer<AccelManager::RasterElement>, raster::VertArgs> const *_draw_id_shader;
    RasterShader<AccelManager::RasterElement, float4x4> const *_contour_draw;
    Shader2D<Image<uint>, uint4> const *_clear_id;
    Shader2D<Image<uint>, Image<float>> const *_shading_id;
    Shader2D<Image<float>, Image<float>, int2, int, float> const *_contour_flood;
    Shader2D<Image<float>, Image<float>, Image<float>, float3> const *_contour_reduce;
    ShaderBase const *_click_pick;

public:
    void on_enable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    void update(Pipeline const &pipeline, PipelineContext const &ctx) override;
    void on_disable(
        Pipeline const &pipeline,
        Device &device,
        CommandList &cmdlist,
        SceneManager &scene) override;
    RasterPass();
    ~RasterPass();
    Image<float> emission;
    void wait_enable() override;
    void contour(PipelineContext const &ctx);
};
struct RasterPassContext : public PassContext {
    DepthBuffer depth_buffer;
    RasterPassContext();
    ~RasterPassContext();
};
}// namespace rbc
RBC_RTTI(rbc::RasterPass)
RBC_RTTI(rbc::RasterPassContext)
