#include <rbc_render/editing_pass.h>

#include <rbc_graphics/scene_manager.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/renderer_data.h>
#include <rbc_graphics/render_device.h>
#include <rbc_render/raster_pass.h>
#include <luisa/backends/ext/raster_ext.hpp>

namespace rbc {
namespace click_pick {
#include <raster/click_pick.inl>
}// namespace click_pick

void EditingPass::on_enable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    VertexAttribute attrs[2] = {
        VertexAttribute{.type = VertexAttributeType::Position, .format = PixelFormat::RGBA32F},
        VertexAttribute{.type = VertexAttributeType::Color, .format = PixelFormat::RGBA32F}};
    _gizmos_mesh_format = {};
    _gizmos_mesh_format.emplace_vertex_stream({attrs, 1});
    _gizmos_mesh_format.emplace_vertex_stream({attrs + 1, 1});

#define RBC_LOAD_SHADER(SHADER_NAME, NAME_SPACE, PATH) \
    _init_counter.add();                               \
    luisa::fiber::schedule([this]() {                  \
        SHADER_NAME = NAME_SPACE::load_shader(PATH);   \
        _init_counter.done();                          \
    })
    RBC_LOAD_SHADER(_click_pick, click_pick, "raster/click_pick.bin");
    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/contour_draw.bin", _contour_draw);
    ShaderManager::instance()->async_load_raster_shader(_init_counter, "raster/draw_gizmos.bin", _draw_gizmos);
    ShaderManager::instance()->async_load(_init_counter, "raster/contour_flood.bin", _contour_flood);
    ShaderManager::instance()->async_load(_init_counter, "path_tracer/clear_buffer.bin", _clear_buffer);
    ShaderManager::instance()->async_load(_init_counter, "raster/contour_reduce.bin", _contour_reduce);
    ShaderManager::instance()->async_load(_init_counter, "raster/draw_frame_selection.bin", _draw_frame_selection);
}
void EditingPass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
}
#undef RBC_LOAD_SHADER
EditingPass::EditingPass() = default;
EditingPass::~EditingPass() = default;
void EditingPass::wait_enable() {
    _init_counter.wait();
}

void EditingPass::contour(PipelineContext const &ctx, luisa::span<uint const> draw_indices) {
    if (draw_indices.empty()) return;
    RasterState raster_state{
        .cull_mode = CullMode::None,
    };
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    luisa::vector<RasterMesh> meshes;
    luisa::vector<geometry::RasterElement> host_data;
    meshes.reserve(draw_indices.size());
    host_data.reserve(draw_indices.size());
    auto elem_buffer = render_device.create_transient_buffer<geometry::RasterElement>("contour_elem_buffer", draw_indices.size());
    for (auto &i : draw_indices) {
        auto draw_cmd = sm.accel_manager().draw_object(i, 1, meshes.size());
        meshes.push_back(std::move(draw_cmd.mesh));
        host_data.push_back(draw_cmd.info);
    }
    auto &cmdlist = *ctx.cmdlist;
    cmdlist << elem_buffer.view(0, host_data.size()).copy_from(host_data.data());
    sm.dispose_after_commit(std::move(host_data));
    const auto &cam_data = ctx.pipeline_settings.read<CameraData>();
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    auto raster_ext = render_device.lc_device().extension<RasterExt>();
    auto origin_map = render_device.create_transient_image<float>(
        "contour_origin",
        PixelStorage::BYTE1,
        frame_settings.render_resolution,
        1, false, true);
    Image<float>
        contour_imgs[2]{
            render_device.create_transient_image<float>(
                "contour_temp0",
                PixelStorage::BYTE1,
                frame_settings.render_resolution),
            render_device.create_transient_image<float>(
                "contour_temp1",
                PixelStorage::BYTE1,
                frame_settings.render_resolution)};
    cmdlist << raster_ext->clear_render_target(
        origin_map,
        float4(0));
    cmdlist << (*_contour_draw)(
                   elem_buffer,
                   cam_data.vp)
                   .draw(
                       std::move(meshes),
                       sm.accel_manager().basic_foramt(),
                       Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, nullptr, origin_map);
    sm.dispose_after_sync(std::move(elem_buffer));
    auto const *src_img = &origin_map;
    for (int i = 0; i < 2; ++i) {
        cmdlist << (*_contour_flood)(
                       *src_img,
                       contour_imgs[1],
                       int2(0, 1),
                       3,
                       2.0f)
                       .dispatch(frame_settings.render_resolution)
                << (*_contour_flood)(
                       contour_imgs[1],
                       contour_imgs[0],
                       int2(1, 0),
                       3,
                       2.0f)
                       .dispatch(frame_settings.render_resolution);
        src_img = &contour_imgs[0];
    }
    cmdlist << (*_contour_reduce)(
                   origin_map,
                   *src_img,
                   *frame_settings.dst_img,
                   float3(1.0f, 1.0f, 1.0f))
                   .dispatch(frame_settings.render_resolution);
}
void EditingPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto &sm = SceneManager::instance();
    auto &render_device = RenderDevice::instance();
    auto &cmdlist = *ctx.cmdlist;
    auto const &frame_settings = ctx.pipeline_settings.read<FrameSettings>();
    auto click_manager = ctx.pipeline_settings.read_if<ClickManager>();
    const auto &cam_data = ctx.pipeline_settings.read<CameraData>();
    auto id_map = render_device.get_transient_image<uint>("id_map", PixelStorage::INT4, frame_settings.render_resolution, 1, false, true);
    if (!id_map) {
        LUISA_ERROR("ID map is missing, must disable editing pass");
        return;
    }
    if (click_manager) {
        click_manager->_mtx.lock();
        auto contour_objects = std::move(click_manager->_contour_objects);
        auto frame_selection = std::move(click_manager->_frame_selection_requires);
        click_manager->_mtx.unlock();
        if (!contour_objects.empty()) {
            contour(ctx, contour_objects);
        }
        if (!click_manager->_gizmos_requires.empty()) {
            auto reqs = std::move(click_manager->_gizmos_requires);
            auto result_buffer = render_device.create_transient_buffer<float4>("gizmos_result", reqs.size());
            cmdlist << (*_clear_buffer)(result_buffer.view().as<uint>(), ~0u).dispatch(result_buffer.size_bytes() / sizeof(uint));
            RasterState raster_state{
                .cull_mode = CullMode::None,
                .depth_state = DepthState{.enable_depth = true, .comparison = Comparison::Greater, .write = true}};

            uint obj_id = 0;
            auto pass_ctx = ctx.mut.get_pass_context<RasterPassContext>();
            if (pass_ctx->depth_buffer && any(pass_ctx->depth_buffer.size() != frame_settings.render_resolution)) {
                sm.dispose_after_sync(std::move(pass_ctx->depth_buffer));
            }
            if (!pass_ctx->depth_buffer) {
                pass_ctx->depth_buffer = render_device.lc_device().create_depth_buffer(DepthFormat::D32, frame_settings.render_resolution);
            }

            cmdlist << pass_ctx->depth_buffer.clear(0.0f);
            for (auto &i : reqs) {
                VertexBufferView vbv[2] = {
                    VertexBufferView{i.pos_buffer},
                    VertexBufferView{i.color_buffer},
                };
                luisa::vector<RasterMesh> raster_mesh;
                raster_mesh.emplace_back(
                    luisa::span{vbv},
                    i.triangle_buffer.as<uint>(),
                    1,
                    obj_id);
                obj_id++;
                PixelArgs pixel_args{
                    .clicked_pixel = i.clicked_pos,
                    .from_mapped_color = i.from_color,
                    .to_mapped_color = i.to_color};
                cmdlist << (*_draw_gizmos)(cam_data.vp * i.transform, result_buffer, pixel_args)
                               .draw(std::move(raster_mesh),
                                     _gizmos_mesh_format,
                                     Viewport{0, 0, frame_settings.render_resolution.x, frame_settings.render_resolution.y}, raster_state, &pass_ctx->depth_buffer,
                                     *frame_settings.dst_img);
            }

            luisa::vector<float4> results;
            results.push_back_uninitialized(result_buffer.size());
            cmdlist << result_buffer.view().copy_to(results.data());
            cmdlist.add_callback([&ctx,
                                  reqs = std::move(reqs),
                                  results = std::move(results)]() mutable {
                auto click_manager = ctx.pipeline_settings.read_if<ClickManager>();
                if (!click_manager) return;
                std::lock_guard lck{click_manager->_mtx};
                for (auto i : vstd::range(reqs.size())) {
                    auto r = results[i];
                    GizmosResult result{
                        .local_pos = r.xyz(),
                        .primitive_id = reinterpret_cast<uint &>(r.w)};
                    click_manager->_gizmos_clicked_result.force_emplace(std::move(reqs[i].name), result);
                }
            });
        }
        if (!click_manager->_requires.empty()) {
            // click
            auto &reqs = click_manager->_requires;
            auto require_buffer = sm.host_upload_buffer().allocate_upload_buffer<float2>(reqs.size());
            auto result_buffer = render_device.create_transient_buffer<RayCastResult>(
                "click_result",
                reqs.size());
            luisa::fixed_vector<float2, 2> req_coords;
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
                auto click_manager = ctx.pipeline_settings.read_if<ClickManager>();
                if (!click_manager) return;
                std::lock_guard lck{click_manager->_mtx};
                for (auto i : vstd::range(reqs.size())) {
                    click_manager->_results.force_emplace(std::move(reqs[i].first), result[i]);
                }
            });
        }
        // frame selection
        luisa::vector<uint> selection_result;
        luisa::spin_mutex selection_mtx;

        for (auto &i : frame_selection) {
            if (length(abs(i.min_projection - i.max_projection)) < 1e-3f) {
                continue;
            }
            auto start_pixel = make_int2((i.min_projection * 0.5f + 0.5f) * make_float2(frame_settings.render_resolution));
            auto end_pixel = make_int2((i.max_projection * 0.5f + 0.5f) * make_float2(frame_settings.render_resolution) + 0.9999f);
            if (i.draw_rectangle) {
                cmdlist << (*_draw_frame_selection)(
                               *frame_settings.dst_img,
                               float4(0, 0, 1, 0.3f),
                               start_pixel)
                               .dispatch(make_uint2(end_pixel - start_pixel));
            }
            selection_result.clear();
            auto select_call = [&](uint user_id, float4x4 const &transform, AABB const &bounding_box) {
                float3 local_min(bounding_box.packed_min[0], bounding_box.packed_min[1], bounding_box.packed_min[2]);
                float3 local_max(bounding_box.packed_max[0], bounding_box.packed_max[1], bounding_box.packed_max[2]);
                float3 proj_min = make_float3(1e20f, 1e20f, 1.f);
                float3 proj_max = make_float3(-1e20f, -1e20f, 0.f);
                for (int x = 0; x < 2; x++)
                    for (int y = 0; y < 2; y++)
                        for (int z = 0; z < 2; z++) {
                            float3 point = select(local_min, local_max, int3(x, y, z) == 1);
                            float4 proj = cam_data.vp * (transform * make_float4(point, 1.f));
                            proj /= proj.w;
                            proj_min = min(proj_min, proj.xyz());
                            proj_max = max(proj_max, proj.xyz());
                        }
                if (all(proj_max < make_float3(i.max_projection, 1e20f) && proj_min > make_float3(i.min_projection, 0.f))) {
                    std::lock_guard lck{selection_mtx};
                    selection_result.push_back(user_id);
                    return true;
                }
                return false;
            };
            sm.accel_manager().iterate_scene(select_call);
            if (!selection_result.empty()) {
                std::lock_guard lck{click_manager->_mtx};
                click_manager->_frame_selection_results.force_emplace(std::move(i.name), std::move(selection_result));
            }
        }
    }
}
}// namespace rbc