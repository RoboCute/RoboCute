#include <rbc_render/offline_pt_pass.h>
#include <rbc_graphics/scene_manager.h>
#include <rbc_render/pipeline.h>
#include <rbc_render/utils/heitz_sobol.h>
#include <rbc_render/accum_pass.h>
#include <rbc_render/renderer_data.h>
#include <rbc_render/editing_pass.h>
#include <rbc_graphics/texture/tex_stream_manager.h>
#include <rbc_graphics/render_device.h>

namespace rbc {
namespace offline_pt_shader {
#include <path_tracer/offline_pt.inl>
}// namespace offline_pt_shader
namespace offline_pt_shader_denoise {
#include <path_tracer/offline_pt_denoise.inl>
}// namespace offline_pt_shader_denoise
namespace offline_multibounce {
#include <path_tracer/pt_multi_bounce_offline.inl>
}// namespace offline_multibounce
namespace ao_trace {
#include <path_tracer/ao_trace.inl>
}// namespace ao_trace
// #define RBC_USE_RAYQUERY
PTPassContext::PTPassContext() = default;
PTPassContext::~PTPassContext() = default;

void OfflinePTPass::on_enable(
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

    auto load = [&](auto name, auto &&var) {
        _init_counter.add();
        luisa::fiber::schedule([this, name, &var]() {
            ShaderManager::instance()->load(name, var);
            _init_counter.done();
        });
    };
    RBC_LOAD_SHADER(_pt_shader, offline_pt_shader, "path_tracer/offline_pt.bin");
    RBC_LOAD_SHADER(_pt_shader_denoise, offline_pt_shader_denoise, "path_tracer/offline_pt_denoise.bin");
    RBC_LOAD_SHADER(_multi_bounce, offline_multibounce, "path_tracer/pt_multi_bounce_offline.bin");
    RBC_LOAD_SHADER(_ao_trace, ao_trace, "path_tracer/ao_trace.bin");
    load("path_tracer/draw_sky.bin", _draw_sky_shader);
    load("surfel/clear_hashgrid_offline.bin", _clear_hashgrid);
    load("surfel/accum_hashgrid_offline.bin", _accum_hashgrid);
    load("surfel/integrate_hashgrid_offline.bin", _integrate_hashgrid);
    load("path_tracer/clear_buffer.bin", _clear_ptr_buffer);
    _init_counter.add();
    luisa::fiber::schedule([&]() {
        auto hash_size = 1024ull * 1024ull * 4ull;
        key_buffer = device.create_buffer<uint>(hash_size);
        value_buffer = device.create_buffer<uint>(hash_size * 4);
        _init_counter.done();
    });
#undef RBC_LOAD_SHADER
}

void OfflinePTPass::early_update(Pipeline const &pipeline, PipelineContext const &ctx) {
    ctx.scene->accel_manager().init_accel(*ctx.cmdlist);
    const auto &pt_settings = ctx.pipeline_settings.read<PathTracerSettings>();
    auto &pipeline_mode = ctx.pipeline_settings.read_mut<PTPipelineSettings>();
    if (pt_settings.enable_ao_mode) {
        pipeline_mode.use_post_filter = false;
    }
}

OfflinePTPass::PreparedResources OfflinePTPass::_prepare_resources(const PTResourceContext &rc) const {
    PreparedResources res;
    res.emission = rc.render_device.create_transient_image<float>(
        "emission", PixelStorage::FLOAT4, rc.frame_settings.render_resolution);

    if (!rc.frame_settings.id_img) {
        res.id_map_val = rc.render_device.create_transient_image<uint>(
            "id_map", PixelStorage::INT4, rc.frame_settings.render_resolution, 1, false, true);
        res.id_map = &res.id_map_val;
    } else {
        res.id_map = rc.frame_settings.id_img;
    }

    res.surfel_mark = rc.render_device.create_transient_image<uint>(
        "surfel_mask", PixelStorage::INT1, rc.frame_settings.render_resolution);

    uint2 res_half = (rc.frame_settings.display_resolution + 1u) / 2u;
    auto buffer_size = res_half.x * res_half.y;
    res.multibounce_buffer = rc.render_device.create_transient_buffer<offline::MultiBouncePixel>(
        "offline_multibounce", buffer_size);
    res.multibounce_buffer_counter = rc.render_device.create_transient_buffer<uint>(
        "offline_multibounce_counter", 1);

    res.geo_buffer = rc.render_device.create_transient_buffer<pt::GBuffer>(
        "offline_geo_buffer",
        rc.frame_settings.display_resolution.x * rc.frame_settings.display_resolution.y);

    return res;
}

offline::PTArgs OfflinePTPass::_setup_pt_args(
    const PTResourceContext &rc,
    const CameraData &cam_data,
    const Camera &cam,
    const SkyHeapIndices &sky_heap,
    bool write_id_map,
    uint32_t frame_index) const {
    offline::PTArgs pt_args{};
    pt_args.write_id_map = write_id_map;
    pt_args.resource_to_rec2020_mat = rc.frame_settings.to_rec2020_matrix;
    pt_args.world_2_sky_mat = cam_data.world_to_sky;
    pt_args.sky_heap_idx = sky_heap.sky_heap_idx;
    pt_args.alias_table_idx = sky_heap.alias_heap_idx;
    pt_args.pdf_table_idx = sky_heap.pdf_heap_idx;
    pt_args.cam_pos = make_float3(cam.position);
    pt_args.inv_view = cam_data.inv_view;
    pt_args.view = cam_data.view;
    pt_args.inv_vp = cam_data.inv_vp;
    pt_args.frame_countdown = rc.scene.tex_streamer().countdown();
    pt_args.light_count = static_cast<uint>(rc.scene.light_accel().light_count());
    pt_args.tex_grad_scale = float2(1);
    pt_args.enable_physical_camera = cam.enable_physical_camera;
    pt_args.require_reject = rc.frame_settings.reject_sampling;
    pt_args.frame_index = frame_index;

    if (cam.enable_physical_camera) {
        pt_args.lens_radius = static_cast<float>(0.05 / cam.aperture);
        pt_args.focus_distance = cam.focus_distance;
    }

    return pt_args;
}

void OfflinePTPass::_draw_sky_only(
    const PTResourceContext &rc,
    const Image<float> &emission,
    Image<uint> const *id_map,
    const SkyHeapIndices &sky_heap,
    const CameraData &cam_data,
    const Camera &cam,
    const JitterData &jitter_data,
    bool write_id_map) const {
    rc.cmdlist << (*_draw_sky_shader)(
                      emission,
                      *id_map,
                      rc.scene.image_heap(),
                      rc.scene.volume_heap(),
                      sky_heap.sky_heap_idx,
                      rc.frame_settings.to_rec2020_matrix,
                      cam_data.world_to_sky,
                      cam_data.inv_vp,
                      make_float3(cam.position),
                      jitter_data.jitter,
                      rc.frame_settings.frame_index,
                      write_id_map)
                      .dispatch(rc.frame_settings.render_resolution);
}

void OfflinePTPass::_clear_multibounce_counter(
    const PTResourceContext &rc,
    const Buffer<uint> &counter) const {
    rc.cmdlist << (*_clear_ptr_buffer)(counter.view(), 0).dispatch(1);
}

void OfflinePTPass::_trace_ao_sample(
    const PTResourceContext &rc,
    const Image<float> &emission,
    const offline::PTArgs &pt_args,
    const PathTracerSettings &pt_settings) const {
    auto &accel = rc.scene.accel();
    rc.cmdlist << ao_trace::dispatch_shader(
        _ao_trace, rc.frame_settings.render_resolution,
        rc.scene.buffer_heap(),
        rc.scene.image_heap(),
        rc.scene.volume_heap(),
        rc.scene.tex_streamer().level_buffer(),
#ifdef RBC_USE_RAYQUERY
        rc.scene.accel_manager().triangle_vis_buffer(),
#endif
        accel,
        emission,
        pt_args,
        pt_settings.ao_max_radius,
        pt_settings.ao_atten_pow,
        pt_settings.ao_use_cosine_sample);
}

void OfflinePTPass::_dispatch_path_tracing(
    const PTResourceContext &rc,
    const PreparedResources &resources,
    offline::PTArgs &pt_args,
    Image<uint> const *id_map,
    uint32_t geometry_mask) const {
    auto &accel = rc.scene.accel();

    // Create a copy of pt_args with the geometry_mask set
    pt_args.geometry_mask = geometry_mask;

    auto geometry_buffer = rc.frame_settings.pt_geometry_buffer ? rc.frame_settings.pt_geometry_buffer : resources.multibounce_buffer_counter.view().as<float>();

    if (rc.frame_settings.albedo_buffer && rc.frame_settings.normal_buffer) {
        rc.cmdlist << offline_pt_shader_denoise::dispatch_shader(
            _pt_shader_denoise,
            ((rc.frame_settings.render_resolution + 1u) / 2u) * 2u,
            rc.scene.tex_streamer().level_buffer(),
            rc.scene.buffer_heap(),
            rc.scene.image_heap(),
            rc.scene.volume_heap(),
#ifdef RBC_USE_RAYQUERY
            rc.scene.accel_manager().triangle_vis_buffer(),
#endif
            accel,
            resources.emission,
            rc.accum_pass_ctx->hdr,
            *id_map,
            resources.geo_buffer.view(),
            *rc.frame_settings.albedo_buffer,
            *rc.frame_settings.normal_buffer,
            geometry_buffer,
            resources.multibounce_buffer.view(),
            resources.multibounce_buffer_counter,
            pt_args,
            rc.frame_settings.render_resolution);
    } else {
        rc.cmdlist << offline_pt_shader::dispatch_shader(
            _pt_shader,
            ((rc.frame_settings.render_resolution + 1u) / 2u) * 2u,
            rc.scene.tex_streamer().level_buffer(),
            rc.scene.buffer_heap(),
            rc.scene.image_heap(),
            rc.scene.volume_heap(),
#ifdef RBC_USE_RAYQUERY
            rc.scene.accel_manager().triangle_vis_buffer(),
#endif
            accel,
            resources.emission,
            rc.accum_pass_ctx->hdr,
            *id_map,
            resources.geo_buffer.view(),
            geometry_buffer,
            resources.multibounce_buffer.view(),
            resources.multibounce_buffer_counter,
            pt_args,
            rc.frame_settings.render_resolution);
    }
}

void OfflinePTPass::_process_multibounce_indirect(
    const PTResourceContext &rc,
    const PreparedResources &resources,
    const offline::PTArgs &pt_args,
    float accumulate_rate) const {
    auto &accel = rc.scene.accel();
    uint max_accum = (1 + pt_args.frame_index) * 1024;

    rc.cmdlist << offline_multibounce::dispatch_shader(
        _multi_bounce,
        resources.multibounce_buffer.view().size(),
        rc.scene.buffer_heap(),
        rc.scene.image_heap(),
        rc.scene.volume_heap(),
        rc.scene.tex_streamer().level_buffer(),
        accel,
#ifdef RBC_USE_RAYQUERY
        rc.scene.accel_manager().triangle_vis_buffer(),
#endif
        resources.multibounce_buffer.view(),
        resources.multibounce_buffer_counter,
        resources.geo_buffer.view(),
        pt_args,
        rc.frame_settings.render_resolution);

    rc.cmdlist << (*_accum_hashgrid)(
                      resources.geo_buffer,
                      key_buffer,
                      value_buffer,
                      resources.surfel_mark,
                      pt_args.jitter_offset,
                      pt_args.cam_pos,
                      10.0f,
                      key_buffer.size(),
                      8,
                      max_accum)
                      .dispatch(rc.frame_settings.render_resolution);

    rc.cmdlist << (*_integrate_hashgrid)(
                      resources.geo_buffer,
                      resources.emission,
                      value_buffer,
                      resources.surfel_mark,
                      max_accum,
                      accumulate_rate)
                      .dispatch(rc.frame_settings.render_resolution);

    rc.cmdlist << (*_clear_hashgrid)(key_buffer, value_buffer, max_accum)
                      .dispatch(key_buffer.size());
}

void OfflinePTPass::update(Pipeline const &pipeline, PipelineContext const &ctx) {
    auto accum_pass_ctx = ctx.mut.get_pass_context<AccumPassContext>();
    auto &frame_settings = ctx.pipeline_settings.read_mut<FrameSettings>();
    auto &scene = *ctx.scene;
    auto &cmdlist = (*ctx.cmdlist);
    auto &render_device = RenderDevice::instance();

    const auto &jitter_data = ctx.pipeline_settings.read<JitterData>();
    const auto &cam_data = ctx.pipeline_settings.read<CameraData>();
    const auto &pt_settings = ctx.pipeline_settings.read<PathTracerSettings>();
    const auto &cam = ctx.pipeline_settings.read<Camera>();
    const auto &sky_heap = ctx.pipeline_settings.read<SkyHeapIndices>();

    auto edit = pipeline.get_pass<EditingPass>();
    bool write_id_map = (edit && edit->actived()) || frame_settings.id_img;

    auto &accel = scene.accel();

    auto &pass_ctx = ctx.mut.get_pass_context_mut<PTPassContext>();
    PTResourceContext rc{
        pipeline, ctx, scene, cmdlist, frame_settings,
        render_device, accum_pass_ctx, pass_ctx};

    auto resources = _prepare_resources(rc);

    if (!accel || accel.size() == 0) {
        _draw_sky_only(rc, resources.emission, resources.id_map, sky_heap,
                       cam_data, cam, jitter_data, write_id_map);
        return;
    }

    if (all(frame_settings.display_resolution == frame_settings.render_resolution) &&
        accum_pass_ctx->frame_index < 64 &&
        frame_settings.reject_sampling) {
        scene.tex_streamer().force_sync();
    }

    if (!pass_ctx) {
        pass_ctx = vstd::make_unique<PTPassContext>();
    }

    auto halton = [](int32_t index, int32_t base) {
        float f = 1.0f, result = 0.0f;
        for (int32_t current_index = index; current_index > 0;) {
            f /= static_cast<float>(base);
            result = result + f * static_cast<float>(current_index % base);
            current_index = static_cast<uint32_t>(floorf(static_cast<float>(current_index) /
                                                         static_cast<float>(base)));
        }
        return result;
    };

    if (accum_pass_ctx->frame_index == 0) {
        cmdlist << (*_clear_hashgrid)(key_buffer, value_buffer, 0).dispatch(key_buffer.size());
    }

    for (auto i : vstd::range(pt_settings.offline_spp)) {
        uint32_t frame_index = accum_pass_ctx->frame_index * pt_settings.offline_spp + i;
        auto pt_args = _setup_pt_args(rc, cam_data, cam, sky_heap, write_id_map, frame_index);
        pt_args.bounce = pt_settings.offline_origin_bounce;
        pt_args.reset_emission = (i == 0);
        pt_args.jitter_offset = float2(
            halton(pt_args.frame_index & 65535, 2),
            halton(pt_args.frame_index & 65535, 3));

        if (pt_settings.enable_ao_mode) {
            _trace_ao_sample(rc, resources.emission, pt_args, pt_settings);
            continue;
        }

        _clear_multibounce_counter(rc, resources.multibounce_buffer_counter);

        if ((bool)frame_settings.albedo_buffer != (bool)frame_settings.normal_buffer) [[unlikely]] {
            LUISA_ERROR("normal_buffer and albedo_buffer must be provided together.");
        }

        const uint geometry_byte_size[] = {
            4, // depth
            12,// normal
            4, // object id
            4, // prim id
            8, // bary
            12,// emission
            12,// albedo
            4, // mat_id
            8, // uv
        };

        uint32_t geometry_mask = 0;
        uint64_t buffer_dst_size = 0;
        if (frame_settings.pt_geometry_buffer) {
            for (auto j : vstd::range(vstd::array_count(geometry_byte_size))) {
                if ((luisa::to_underlying(frame_settings.geometry_channel) & (1 << j)) == 0) {
                    continue;
                }
                buffer_dst_size += geometry_byte_size[j];
                geometry_mask |= (1 << j);
            }
            auto desired_size = buffer_dst_size * frame_settings.render_resolution.x *
                                frame_settings.render_resolution.y;
            if (frame_settings.pt_geometry_buffer.size_bytes() < desired_size) [[unlikely]] {
                LUISA_ERROR(
                    "Geometry buffer size {} less than desired size (dest_size_bytes {}) x (width {}) x (height {}) = {}",
                    frame_settings.pt_geometry_buffer.size_bytes(),
                    buffer_dst_size,
                    frame_settings.render_resolution.x,
                    frame_settings.render_resolution.y,
                    desired_size);
            }
        }

        if (frame_settings.albedo_buffer && frame_settings.normal_buffer) {
            auto desired_buffer_size = frame_settings.render_resolution.x *
                                       frame_settings.render_resolution.y * 3 * sizeof(float);
            if (frame_settings.albedo_buffer->size_bytes() != desired_buffer_size ||
                frame_settings.normal_buffer->size_bytes() != desired_buffer_size) [[unlikely]] {
                LUISA_ERROR("Buffer size mismatch.");
            }
        }

        _dispatch_path_tracing(rc, resources, pt_args, resources.id_map, geometry_mask);

        if (pt_settings.offline_indirect_bounce > 0) {
            pt_args.bounce = pt_settings.offline_indirect_bounce;
            float accumulate_rate = (i == (pt_settings.offline_spp - 1)) ? (1.0f / static_cast<float>(pt_settings.offline_spp)) : 1.0f;
            _process_multibounce_indirect(rc, resources, pt_args, accumulate_rate);
        }
    }

    frame_settings.albedo_buffer = nullptr;
    frame_settings.normal_buffer = nullptr;
}

void OfflinePTPass::on_frame_end(
    Pipeline const &pipeline,
    Device &device,
    SceneManager &scene) {
}

void OfflinePTPass::on_disable(
    Pipeline const &pipeline,
    Device &device,
    CommandList &cmdlist,
    SceneManager &scene) {
    key_buffer = {};
    value_buffer = {};
}

void OfflinePTPass::wait_enable() {
    _init_counter.wait();
}

OfflinePTPass::~OfflinePTPass() = default;

}// namespace rbc

#undef RBC_USE_RAYQUERY