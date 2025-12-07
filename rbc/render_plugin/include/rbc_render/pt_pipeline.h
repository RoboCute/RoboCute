#pragma once
#include <rbc_render/pipeline.h>
#include <rbc_render/generated/pipeline_settings.hpp>
#include <rbc_render/procedural/sky_atmosphere.h>

namespace rbc {
struct RasterPass;
struct OfflinePTPass;
struct AccumPass;
struct PostPass;
struct EditingPass;
struct PTPipeline : public rbc::Pipeline {
    PTPipeline();
    ~PTPipeline();
    void initialize() override;
    void update(rbc::PipelineContext &ctx) override;
    void early_update(rbc::PipelineContext &ctx) override;
    // passes
    rbc::PreparePass *prepare_pass = nullptr;
    rbc::RasterPass *raster_pass = nullptr;
    rbc::OfflinePTPass *pt_pass = nullptr;
    rbc::AccumPass *accum_pass = nullptr;
    rbc::PostPass *post_pass = nullptr;
    rbc::EditingPass *editing_pass = nullptr;

    // monitor info
};
}// namespace rbc