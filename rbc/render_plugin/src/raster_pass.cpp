#include <rbc_render/raster_pass.h>
#include <rbc_graphics/frustum.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/render_device.h>
#include <luisa/backends/ext/raster_ext.hpp>
namespace rbc {
void RasterPass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    _init_counter.add();
    luisa::fiber::schedule([this] {
        _draw_scene_shader = ShaderManager::instance()->load_raster_shader<Buffer<AccelManager::RasterElement>, raster::VertArgs>("raster/test_raster.bin");
        _init_counter.done();
    });
}
void RasterPass::wait_enable() {
    _init_counter.wait();
}

void RasterPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto pass_ctx = ctx.mut.get_pass_context<RasterPassContext>();
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    const auto &frameSettings = ctx.pipeline_settings->read<FrameSettings>();
    const auto &cam_data = ctx.pipeline_settings->read<CameraData>();
    if (pass_ctx->depth_buffer && any(pass_ctx->depth_buffer.size() != frameSettings.render_resolution)) {
        sm.dispose_after_sync(std::move(pass_ctx->depth_buffer));
    }
    if (!pass_ctx->depth_buffer) {
        pass_ctx->depth_buffer = render_device.lc_device().create_depth_buffer(DepthFormat::D32, frameSettings.render_resolution);
    }
    AccelManager::DrawListMap draw_meshes;
    BufferView<AccelManager::RasterElement> data_buffer;
    auto &cmdlist = *ctx.cmdlist;
    auto frustum_corners = ctx.cam.frustum_corners();
    auto frustum_planes = ctx.cam.frustum_plane();
    auto frustum_min_point = frustum_corners[0];
    auto frustum_max_point = frustum_corners[0];
    for (auto i : vstd::range(1, 8)) {
        frustum_min_point = min(frustum_min_point, frustum_corners[i]);
        frustum_max_point = max(frustum_max_point, frustum_corners[i]);
    }
    static bool bb = false;
    // if (!bb)
    // LUISA_INFO("min {} max {}", frustum_min_point, frustum_max_point);
    // bb = true;
    auto frustum_cull_callback = [&](float4x4 const &transform, AABB const &bounding) {
        return frustum_cull(transform, bounding, frustum_planes, frustum_min_point, frustum_max_point, ctx.cam.dir_forward(), make_float3(ctx.cam.position));
        //  return true;
    };
    sm.accel_manager().make_draw_list(
        cmdlist,
        sm.after_commit_dsp_queue(),
        sm.dispose_queue(),
        std::move(frustum_cull_callback),
        draw_meshes, data_buffer);
    RasterState raster_state{
        .cull_mode = CullMode::None,
        .depth_state = DepthState{
            .enable_depth = true,
            .comparison = Comparison::Greater,
            .write = true},
    };
    auto raster_ext = render_device.lc_device().extension<RasterExt>();
    emission = render_device.create_transient_image<float>("emission", PixelStorage::HALF4, frameSettings.render_resolution, 1, false, true);
    cmdlist << raster_ext->clear_render_target(emission, float4(0.3f, 0.6f, 0.7f, 1.f))
            << pass_ctx->depth_buffer.clear(0.0f);
    //
    if (!draw_meshes.empty()) {
        raster::VertArgs vert_args{
            .view = cam_data.view,
            .proj = cam_data.proj,
            .view_proj = cam_data.vp};
        cmdlist << _draw_scene_shader(data_buffer, vert_args)
                       .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frameSettings.render_resolution.x, frameSettings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, emission);
    }

    ctx.mut.resolved_img = &emission;
}
void RasterPass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {}
RasterPass::RasterPass() {}
RasterPass::~RasterPass() {}
RasterPassContext::RasterPassContext() {}
RasterPassContext::~RasterPassContext() {}
}// namespace rbc
