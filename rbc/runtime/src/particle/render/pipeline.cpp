/**
 * @file pipeline.cpp
 * @brief The Implementation of Pipeline
 * @author sailing-innocent
 * @date 2024-05-16
 */

#include "rbc_particle/render/pipeline.h"
#include "rbc_particle/system.h"

namespace rbc::ps {

PSPipeline::PSPipeline(PSPipeline &&) {
    m_name = std::move(m_name);
    m_passes = std::move(m_passes);
}

PSPipeline::PSPipeline(vstd::string name)
    : m_name(std::move(name)) {
}
PSPipeline::~PSPipeline() = default;

PSPass *PSPipeline::emplace_instance(eastl::unique_ptr<PSPass> &&component, luisa::string &&name) {
    auto *ptr = component.get();
    auto idx = m_passes.size();
    auto &arr = m_pass_indices.emplace(std::move(name)).value();
    arr.emplace_back(idx);
    m_passes.emplace_back(std::move(component));
    return ptr;
}

void PSPipeline::enable(Device &device, CommandList &cmdlist, Scene &scene, uint2 resolution) noexcept {
    for (auto &i : m_passes) {
        i->on_enable(*this, device, cmdlist, scene);
    }
}

void PSPipeline::early_update(PSPipelineContext &ctx, uint64_t mask) noexcept {
    for (auto &i : m_passes) {
        // if (i->valid(mask))
        i->early_update(*this, ctx);
    }
}

void PSPipeline::update(PSPipelineContext &ctx) noexcept {
    assert(m_passes.size() == m_pipeline_enabled.size());
    auto iter = m_pipeline_enabled.begin();
    for (auto &i : m_passes) {
        // if (*iter)
        i->update(*this, ctx);
        ++iter;
    }
    iter = m_pipeline_enabled.begin();
    // for (auto& i : m_passes)
    // {
    //     if (*iter)
    //         i->on_frame_end(*this, ctx.device, ctx.scene);
    //     ++iter;
    // }
}

void PSPipeline::disable(Device &device, CommandList &cmdlist, Scene &scene) noexcept {}

}// namespace rbc::ps