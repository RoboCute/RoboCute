#pragma once
#include <rbc_render/pass.h>
#include <luisa/runtime/shader.h>
#include <luisa/runtime/raster/depth_buffer.h>
#include <luisa/core/fiber.h>
#include <rbc_render/pipeline_context.h>
#include <raster/raster_args.hpp>
namespace rbc
{
struct  RasterPass : public Pass { // struct Reservoir
private:
    luisa::fiber::counter _init_counter;
    RasterShader<Buffer<AccelManager::RasterElement>, raster::VertArgs> _draw_scene_shader;

public:
    void on_enable(
        Pipeline const& pipeline,
        Device& device,
        CommandList& cmdlist,
        SceneManager& scene
    ) override;
    void update(Pipeline const& pipeline, PipelineContext const& ctx) override;
    void on_disable(
        Pipeline const& pipeline,
        Device& device,
        CommandList& cmdlist,
        SceneManager& scene
    ) override;
    RasterPass();
    ~RasterPass();
    Image<float> emission;
};
struct  RasterPassContext : public PassContext {
    DepthBuffer depth_buffer;
    RasterPassContext();
    ~RasterPassContext();
};
} // namespace rbc
RBC_RTTI(rbc::RasterPass)
RBC_RTTI(rbc::RasterPassContext)
