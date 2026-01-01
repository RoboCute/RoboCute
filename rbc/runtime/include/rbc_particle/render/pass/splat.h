#pragma once
/**
 * @file splat.h
 * @brief The Splat Pass
 * @author sailing-innocent
 * @date 2024-05-20
 */

#include "rbc_particle/render/pass.h"
#include "rbc_particle/render/pipeline_context.h"
#include <luisa/luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

namespace rbc::ps
{

struct SplatPSPassContext : public PSPassContext {
    eastl::vector<int> emitter_scene_node_indices;
};

class SplatPSPass : public PSPass
{
    Shader<1, Image<float>, Buffer<float>, int, int> m_splat_shader;
    SplatPSPassContext*                              mp_ctx;

public:
    void on_enable(PSPipeline const& pipeline, Device& device, CommandList& cmdlist, Scene& scene) override;
    void early_update(PSPipeline const& pipeline, PSPipelineContext& ctx) override;
    void update(PSPipeline const& pipeline, PSPipelineContext& ctx) override;
    void on_disable(PSPipeline const& pipeline) override;
    ~SplatPSPass();
};

} // namespace d6::ps