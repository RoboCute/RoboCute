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
    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/accel_id.bin", _draw_id_shader);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/clear_id.bin",
        _clear_id);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/shading_id.bin",
        _shading_id);
}
void RasterPass::wait_enable() {
    _init_counter.wait();
}

void RasterPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto pass_ctx = ctx.mut.get_pass_context<RasterPassContext>();
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    const auto &cam_data = ctx.pipeline_settings.read<CameraData>();
    if (pass_ctx->depth_buffer && any(pass_ctx->depth_buffer.size() != frame_settings.render_resolution)) {
        sm.dispose_after_sync(std::move(pass_ctx->depth_buffer));
    }
    if (!pass_ctx->depth_buffer) {
        pass_ctx->depth_buffer = render_device.lc_device().create_depth_buffer(DepthFormat::D32, frame_settings.render_resolution);
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
    auto frustum_cull_callback = [&](float4x4 const &transform, AABB const &bounding) {
        return frustum_cull(transform, bounding, frustum_planes, frustum_min_point, frustum_max_point, ctx.cam.dir_forward(), make_float3(ctx.cam.position));
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
    emission = render_device.create_transient_image<float>("emission", PixelStorage::BYTE4, frame_settings.render_resolution, 1, false, true);
    frame_settings.resolved_img = &emission;
    auto id_map = render_device.create_transient_image<uint>("id_map", PixelStorage::INT4, frame_settings.render_resolution, 1, false, true);
    cmdlist << pass_ctx->depth_buffer.clear(0.0f)
            << (*_clear_id)(id_map, uint4(-1, -1, 0, 0)).dispatch(frame_settings.render_resolution);
    //
    if (!draw_meshes.empty()) {
        raster::VertArgs vert_args{
            .view = cam_data.view,
            .proj = cam_data.proj,
            .view_proj = cam_data.vp};
        cmdlist << (*_draw_id_shader)(data_buffer, vert_args)
                       .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, id_map);
    }
    cmdlist << (*_shading_id)(id_map, emission).dispatch(frame_settings.render_resolution);
 
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
