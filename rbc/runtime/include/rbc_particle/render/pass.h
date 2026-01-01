#pragma once

/**
 * @file pass.h
 * @brief The Custom Render Pass for Particle System
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include <luisa/vstl/common.h>
#include "rbc_particle/render/pipeline_context.h"

namespace rbc::ps
{
class PSPipeline;
class PSPass : public vstd::IOperatorNewBase
{
public:
    virtual ~PSPass() = default;
    // necessary override: init, update, dispose
    // optional override: early_update, on_frame_end
    virtual void on_enable(PSPipeline const& pipeline, Device& device, CommandList& cmdlist, Scene& scene) = 0;
    virtual void early_update(PSPipeline const& pipeline, PSPipelineContext& ctx) {}
    virtual void update(PSPipeline const& pipeline, PSPipelineContext& ctx) = 0;
    virtual void on_frame_end(PSPipeline const& pipeline) {}
    virtual void on_disable(PSPipeline const& pipeline) = 0;
    virtual bool valid(uint64_t pipeline_mask) const { return true; }
};

} // namespace rbc::ps