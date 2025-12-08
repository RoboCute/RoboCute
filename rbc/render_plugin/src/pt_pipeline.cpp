#include <rbc_render/renderer_data.h>
#include <rbc_render/pt_pipeline.h>
#include <rbc_render/post_pass.h>
#include <rbc_render/prepare_pass.h>
#include <rbc_render/offline_pt_pass.h>
#include <rbc_render/accum_pass.h>
#include <rbc_render/raster_pass.h>
#include <rbc_render/editing_pass.h>
#include <rbc_graphics/render_device.h>
// TODO: test hdri
#include <luisa/core/platform.h>

namespace rbc {
PTPipeline::PTPipeline() = default;

void PTPipeline::initialize() {
    auto &device = RenderDevice::instance();

    prepare_pass = this->emplace_instance<PreparePass>();
    // create passes
    pt_pass = this->emplace_instance<OfflinePTPass>();
    accum_pass = this->emplace_instance<AccumPass>();
    raster_pass = this->emplace_instance<RasterPass>();
    post_pass = this->emplace_instance<PostPass>(device.lc_device_ext());
    editing_pass = this->emplace_instance<EditingPass>();
    // enable passes
    {
        this->enable(
            device.lc_device(),
            device.lc_main_cmd_list(),
            SceneManager::instance());
    }
}

void PTPipeline::update(rbc::PipelineContext &ctx) {
    this->rbc::Pipeline::update(ctx);
}

PTPipeline::~PTPipeline() {
}

void PTPipeline::early_update(rbc::PipelineContext &ctx) {
    // HDR
    // sync monitor
    //     tms.lpm.displayRedPrimary = monitor_info->red_primary;
    // tms.lpm.displayGreenPrimary = monitor_info->green_primary;
    // tms.lpm.displayBluePrimary = monitor_info->blue_primary;
    // tms.lpm.displayWhitePoint = monitor_info->white_point;
    // tms.lpm.displayMinLuminance = monitor_info->min_luminance;
    // tms.lpm.displayMaxLuminance = monitor_info->max_luminance;
    // tms.aces.tone_mapping.hdr_display_multiplier = monitor_info->max_luminance / 80.0f;

    // get settings
    auto &sky_settings = ctx.pipeline_settings->read_mut<SkySettings>();
    auto &frame_settings = ctx.pipeline_settings->read_mut<FrameSettings>();
    auto &sky_heap = ctx.pipeline_settings->read_mut<SkyHeapIndices>();
    // update atom
    if (sky_settings.sky_atom) {
        auto &sky_atom = *sky_settings.sky_atom;

        // update sky matrix
        {
            auto &camera_settings = ctx.pipeline_settings->read_mut<CameraData>();
            luisa::float3x3 &sky_matrix = camera_settings.world_to_sky;
            auto sky_radians = radians(sky_settings.sky_angle);
            sky_matrix.cols[0] = float3(cos(sky_radians), 0.0f, sin(sky_radians));
            sky_matrix.cols[1] = float3(0, 1, 0);
            sky_matrix.cols[2] = normalize(cross(sky_matrix.cols[0], sky_matrix.cols[1]));
            sky_matrix = transpose(sky_matrix);
        }

        // update sky atom
        if (sky_settings.dirty) {
            if (sky_settings.sky_max_lum < 65530)
                sky_atom.clamp_light(*ctx.cmdlist, sky_settings.sky_max_lum, 32);
            else
                sky_atom.copy_img(*ctx.cmdlist);
            sky_atom.colored(*ctx.cmdlist, sky_settings.sky_color);
            sky_atom.mark_dirty();
            double sample_pdf = 1.0 / (2.0 * (double)pi * (1.0 - cos(radians(sky_settings.sun_angle))));
            sample_pdf /= 65536.0f;

            float3 sun_radiance = sky_settings.sun_color * sky_settings.sun_intensity * (float)sample_pdf;
            float len = sun_radiance.x + sun_radiance.y + sun_radiance.z;
            if (length(sky_settings.sun_dir) < 1e-5f) {
                sky_settings.sun_dir = float3(0, -1, 0);
            }
            if (len > 1e-4f) {
                sky_atom.make_sun(
                    *ctx.cmdlist,
                    sky_settings.sun_angle,
                    sun_radiance,
                    normalize(sky_settings.sun_dir));
            }

            sky_settings.dirty = false;
        }
        if (sky_atom.update(*ctx.cmdlist, *ctx.stream, ctx.scene->bindless_allocator(), sky_settings.force_sync)) {
            // frame_settings.sky_confidence = 1.0f;
            frame_settings.frame_index = 0;
        }
        sky_heap.sky_heap_idx = sky_atom.sky_id();
        sky_heap.alias_heap_idx = sky_atom.sky_alias_id();
        sky_heap.pdf_heap_idx = sky_atom.sky_pdf_id();
    } else {
        sky_heap.sky_heap_idx = ~0u;
        sky_heap.alias_heap_idx = ~0u;
        sky_heap.pdf_heap_idx = ~0u;
    }

    // update camera settings
    ctx.cam.set_aspect_ratio_from_resolution(frame_settings.render_resolution.x, frame_settings.render_resolution.y);
    auto &pt_pipe_settings = ctx.pipeline_settings->read_mut<PTPipelineSettings>();
    if(pt_pipe_settings.use_raster && pt_pipe_settings.use_raytracing) [[unlikely]] {
        LUISA_ERROR("Can not enable both raster and raytracing.");
    }
    raster_pass->set_actived(pt_pipe_settings.use_raster);
    editing_pass->set_actived(pt_pipe_settings.use_editing);
    pt_pass->set_actived(pt_pipe_settings.use_raytracing);
    accum_pass->set_actived(pt_pipe_settings.use_raytracing);
    this->rbc::Pipeline::early_update(ctx);
}

}// namespace rbc