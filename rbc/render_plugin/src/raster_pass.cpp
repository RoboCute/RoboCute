#include <rbc_render/raster_pass.h>
#include <rbc_graphics/frustum.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/render_device.h>
#include <luisa/backends/ext/raster_ext.hpp>
namespace rbc {
namespace click_pick {
#include <raster/click_pick.inl>
}// namespace click_pick
void RasterPass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
#define RBC_LOAD_SHADER(SHADER_NAME, NAME_SPACE, PATH) \
    _init_counter.add();                               \
    luisa::fiber::schedule([this]() {                  \
        SHADER_NAME = NAME_SPACE::load_shader(PATH);   \
        _init_counter.done();                          \
    })
    // _init_counter.add();
    // luisa::fiber::schedule([this] {
    //     _draw_scene_shader =
    //         ShaderManager::instance()
    //             ->load_raster_shader<Buffer<AccelManager::RasterElement>, raster::VertArgs>("raster/test_raster.bin");
    //     _init_counter.done();
    // });
    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/accel_id.bin", _draw_id_shader);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/clear_id.bin",
        _clear_id);
    ShaderManager::instance()->async_load(
        _init_counter,
        "raster/shading_id.bin",
        _shading_id);
    ShaderManager::instance()->async_load_raster_shader(_init_counter,  "raster/contour_draw.bin", _contour_draw);
    ShaderManager::instance()->async_load(_init_counter, "raster/contour_flood.bin", _contour_flood);
    ShaderManager::instance()->async_load(_init_counter, "raster/contour_reduce.bin", _contour_reduce);
    RBC_LOAD_SHADER(_click_pick, click_pick, "raster/click_pick.bin");
}
#undef RBC_LOAD_SHADER
void RasterPass::wait_enable() {
    _init_counter.wait();
}

void RasterPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto pass_ctx = ctx.mut.get_pass_context<RasterPassContext>();
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &frameSettings = ctx.pipeline_settings->read_mut<FrameSettings>();
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
    auto id_map = render_device.create_transient_image<uint>("id_map", PixelStorage::INT4, frameSettings.render_resolution, 1, false, true);
    emission = render_device.create_transient_image<float>("emission", PixelStorage::HALF4, frameSettings.render_resolution, 1, false, true);
    cmdlist << pass_ctx->depth_buffer.clear(0.0f)
            << (*_clear_id)(id_map, uint4(-1, -1, 0, 0)).dispatch(frameSettings.render_resolution);
    //
    if (!draw_meshes.empty()) {
        raster::VertArgs vert_args{
            .view = cam_data.view,
            .proj = cam_data.proj,
            .view_proj = cam_data.vp};
        // cmdlist << _draw_scene_shader(data_buffer, vert_args)
        //                .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frameSettings.render_resolution.x, frameSettings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, emission);
        cmdlist << _draw_id_shader(data_buffer, vert_args)
                       .draw(std::move(draw_meshes), sm.accel_manager().basic_foramt(), Viewport{0, 0, frameSettings.render_resolution.x, frameSettings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer, id_map);
    }
    cmdlist << (*_shading_id)(id_map, emission).dispatch(frameSettings.render_resolution);
    frameSettings.resolved_img = &emission;
    auto click_manager = ctx.pipeline_settings->read_if<ClickManager>();
    if (click_manager && !click_manager->_requires.empty()) {
        auto &reqs = click_manager->_requires;
        auto require_buffer = sm.host_upload_buffer().allocate_upload_buffer<float2>(reqs.size());
        auto result_buffer = render_device.create_transient_buffer<RayCastResult>(
            "click_result",
            reqs.size());
        luisa::fixed_vector<float2, 4> req_coords;
        luisa::vector<RayCastResult> result;
        result.push_back_uninitialized(reqs.size());
        vstd::push_back_func(req_coords, reqs.size(), [&](size_t i) {
            return reqs[i].second.screen_uv;
        });
        std::memcpy(require_buffer.mapped_ptr(), req_coords.data(), req_coords.size_bytes());
        cmdlist << click_pick::dispatch_shader(_click_pick, reqs.size(), sm.buffer_heap(), require_buffer.view, id_map, result_buffer.view())
                << result_buffer.view().copy_to(result.data());
        cmdlist.add_callback([&ctx,
                              reqs = std::move(reqs),
                              result = std::move(result)]() mutable {
            auto click_manager = ctx.pipeline_settings->read_if<ClickManager>();
            if (!click_manager) return;
            std::lock_guard lck{click_manager->_mtx};
            for (auto i : vstd::range(reqs.size())) {
                click_manager->_results.force_emplace(std::move(reqs[i].first), result[i]);
            }
        });
    }
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
